/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/BuiltinWrappers.h>
#include <Kernel/Boot/CommandLine.h>
#include <Kernel/Bus/PCI/API.h>
#include <Kernel/Devices/Storage/IDE/DMACompletion.h>
#include <Kernel/Devices/Storage/IDE/IDEChannel.h>
#include <Kernel/Devices/Storage/IDE/IDEController.h>
#include <Kernel/Devices/Storage/IDE/IDEDevice.h>
#include <Kernel/Devices/Storage/IDE/VIAUDMAMode.h>
#include <Kernel/Devices/Storage/StorageManagement.h>
#include <Kernel/Library/IOWindow.h>

namespace Kernel {

// PCI IDE Controller Specification 1.0, programming interface byte.
static constexpr u8 channel_native_mode(u8 channel_index) { return 1 << (channel_index * 2); }
static constexpr u8 channel_mode_switchable(u8 channel_index) { return 1 << (channel_index * 2 + 1); }
static constexpr u8 bus_master_capable = 1 << 7;

static constexpr u16 via_vendor_id = 0x1106;
// VIA UDMA100 south bridges (VT8231 and relatives) hold one Ultra DMA timing byte per drive at
// 0x53 (primary master) down to 0x50 (secondary slave), as in Linux's pata_via.
static constexpr u8 via_udma_timing_register = 0x50;
static constexpr u8 via_udma_cable_bit = 1 << 4;
static constexpr u8 via_udma_enable_bits = 0xe0;
// Cycle time fields for a 33 MHz PCI clock: ceil(mode cycle time / 10 ns) clamped to 2..7, minus 2.
static constexpr Array<u8, 6> via_udma100_cycle_fields { 5, 5, 4, 3, 1, 0 };
static IDEController::IRQOverride s_irq_override;

void IDEController::set_irq_override(IRQOverride override)
{
    VERIFY(!s_irq_override);
    s_irq_override = override;
}
// Modes above this need an 80-wire cable.

static u8 via_udma_register_offset(u8 channel_index, IDEChannel::Drive drive)
{
    return via_udma_timing_register + 3 - (channel_index * 2 + to_underlying(drive));
}

static u8 read_config8(PCI::DeviceIdentifier const& identifier, u8 offset)
{
    SpinlockLocker locker(identifier.operation_lock());
    return PCI::read8_locked(identifier, static_cast<PCI::RegisterOffset>(offset));
}

static void write_config8(PCI::DeviceIdentifier const& identifier, u8 offset, u8 value)
{
    SpinlockLocker locker(identifier.operation_lock());
    PCI::write8_locked(identifier, static_cast<PCI::RegisterOffset>(offset), value);
}

static u8 highest_mode(u8 mask)
{
    VERIFY(mask != 0);
    return 7 - count_leading_zeroes(mask);
}

bool IDEController::is_handled(PCI::DeviceIdentifier const& identifier)
{
    return identifier.class_code() == PCI::ClassID::MassStorage
        && identifier.subclass_code() == to_underlying(PCI::MassStorage::SubclassID::IDEController);
}

ErrorOr<NonnullRefPtr<IDEController>> IDEController::initialize(PCI::DeviceIdentifier const& identifier)
{
    auto controller = TRY(adopt_nonnull_ref_or_enomem(new (nothrow) IDEController()));
    PCI::enable_io_space(identifier);
    // Prefer polling the bus-master latch if the PCI function can mask its
    // CPU interrupt. Controllers without that mask use a registered IRQ.
    PCI::disable_interrupt_line(identifier);
    if (identifier.hardware_id().vendor_id == via_vendor_id) {
        dmesgln("IDE: VIA timing registers: drive timing {:#02x} {:#02x} {:#02x} {:#02x}, Ultra DMA {:#02x} {:#02x} {:#02x} {:#02x}",
            read_config8(identifier, 0x48), read_config8(identifier, 0x49), read_config8(identifier, 0x4a), read_config8(identifier, 0x4b),
            read_config8(identifier, 0x50), read_config8(identifier, 0x51), read_config8(identifier, 0x52), read_config8(identifier, 0x53));
    }
    for (u8 channel_index = 0; channel_index < 2; ++channel_index) {
        if (auto result = controller->initialize_channel(identifier, channel_index); result.is_error())
            dmesgln("IDE: {} channel unavailable: {}", channel_index == 0 ? "Primary"sv : "Secondary"sv, result.error());
    }
    return controller;
}

IDEController::IDEController()
    : StorageController(StorageManagement::generate_controller_id())
{
}

ErrorOr<void> IDEController::initialize_channel(PCI::DeviceIdentifier const& identifier, u8 channel_index)
{
    {
        SpinlockLocker locker(identifier.operation_lock());
        u8 programming_interface = PCI::read8_locked(identifier, PCI::RegisterOffset::PROG_IF);
        if (!(programming_interface & channel_native_mode(channel_index))) {
            // Legacy ISA port decoding is only reachable through x86 port I/O, so prefer native mode.
            if (!(programming_interface & channel_mode_switchable(channel_index)))
                return Error::from_errno(ENOTSUP);
            PCI::write8_locked(identifier, PCI::RegisterOffset::PROG_IF, programming_interface | channel_native_mode(channel_index));
            programming_interface = PCI::read8_locked(identifier, PCI::RegisterOffset::PROG_IF);
            if (!(programming_interface & channel_native_mode(channel_index)))
                return Error::from_errno(ENOTSUP);
        }
    }

    auto command_bar = channel_index == 0 ? PCI::HeaderType0BaseRegister::BAR0 : PCI::HeaderType0BaseRegister::BAR2;
    auto control_bar = channel_index == 0 ? PCI::HeaderType0BaseRegister::BAR1 : PCI::HeaderType0BaseRegister::BAR3;
    dmesgln("IDE: {} {} channel {} in native mode, command block {:#x}, control block {:#x}", identifier.address(), identifier.hardware_id(),
        channel_index, PCI::get_BAR(identifier, command_bar), PCI::get_BAR(identifier, control_bar));

    auto command_block = TRY(IOWindow::create_for_pci_device_bar(identifier, command_bar, 8));
    // The device control and alternate status register is at offset 2 of the 4-byte control block.
    auto control_block_base = TRY(IOWindow::create_for_pci_device_bar(identifier, control_bar, 4));
    auto control_block = TRY(control_block_base->create_from_io_window_with_offset(2));
    OwnPtr<IOWindow> bus_master;
    Optional<InterruptNumber> dma_irq;
    if (s_irq_override)
        dma_irq = s_irq_override(identifier, channel_index);
    if (!dma_irq.has_value()) {
        auto line = identifier.interrupt_line().value();
        if (line > 0 && line != 2 && line < 16)
            dma_irq = InterruptNumber { line };
    }
    u8 programming_interface = read_config8(identifier, to_underlying(PCI::RegisterOffset::PROG_IF));
    bool pci_interrupt_is_masked;
    {
        SpinlockLocker locker(identifier.operation_lock());
        pci_interrupt_is_masked = ide_dma_pin_interrupt_is_masked(PCI::read16_locked(identifier, PCI::RegisterOffset::COMMAND));
    }
    if (channel_index == 0)
        dmesgln("IDE: PCI interrupt output masked for polled DMA: {}", pci_interrupt_is_masked);
    if (kernel_command_line().contains("nodma"sv)) {
        dmesgln("IDE: DMA disabled by the nodma boot argument");
    } else if (!pci_interrupt_is_masked && !dma_irq.has_value()) {
        dmesgln("IDE: PCI interrupt cannot be masked and IRQ {} is invalid, using PIO", identifier.interrupt_line().value());
    } else if (!(programming_interface & bus_master_capable) || PCI::get_BAR(identifier, PCI::HeaderType0BaseRegister::BAR4) == 0) {
        dmesgln("IDE: Controller has no bus master, using PIO");
    } else {
        auto bus_master_or_error = IOWindow::create_for_pci_device_bar(identifier, PCI::HeaderType0BaseRegister::BAR4, 16);
        if (bus_master_or_error.is_error()) {
            dmesgln("IDE: Using PIO, the bus master registers are unavailable: {}", bus_master_or_error.error());
        } else {
            auto channel_window = bus_master_or_error.value()->create_from_io_window_with_offset(channel_index * 8, 8);
            if (channel_window.is_error())
                dmesgln("IDE: Using PIO, the bus master registers are unavailable: {}", channel_window.error());
            else
                bus_master = channel_window.release_value();
        }
    }
    if (pci_interrupt_is_masked)
        dma_irq.clear();
    if (bus_master && dma_irq.has_value()) {
        dmesgln("IDE: channel {} using IRQ {} for DMA (PCI line {})", channel_index, dma_irq->value(), identifier.interrupt_line().value());
    }
    if (bus_master)
        PCI::enable_bus_mastering(identifier);
    auto channel = TRY(IDEChannel::create(move(command_block), move(control_block), move(bus_master), dma_irq));

    for (auto drive : { IDEChannel::Drive::Master, IDEChannel::Drive::Slave }) {
        auto identity = TRY(channel->identify(drive));
        if (!identity.has_value())
            continue;
        configure_dma(identifier, *channel, channel_index, drive, identity.value());
        StorageDevice::LUNAddress lun { controller_id(), channel_index, to_underlying(drive) };
        auto device = TRY(IDEDevice::create(channel, drive, identity.value(), lun, hardware_relative_controller_id()));
        TRY(m_devices.try_append(move(device)));
    }
    return {};
}

void IDEController::configure_dma(PCI::DeviceIdentifier const& identifier, IDEChannel& channel, u8 channel_index, IDEChannel::Drive drive, IDEChannel::Identity const& identity)
{
    auto drive_name = drive == IDEChannel::Drive::Master ? "Master"sv : "Slave"sv;
    if (!channel.has_bus_master())
        return;
    if (!identity.supports_dma || identity.supported_udma_modes == 0) {
        dmesgln("IDE: {} drive has no Ultra DMA modes, using PIO", drive_name);
        return;
    }
    bool is_via = identifier.hardware_id().vendor_id == via_vendor_id;
    u8 mode;
    if (is_via) {
        u8 offset = via_udma_register_offset(channel_index, drive);
        u8 old_timing = read_config8(identifier, offset);
        Optional<u8> requested_mode;
        if (auto option = kernel_command_line().lookup("udma"sv); option.has_value()) {
            auto parsed = option->to_number<unsigned>();
            if (!parsed.has_value() || parsed.value() > 5) {
                dmesgln("IDE: Invalid udma mode, using PIO");
                return;
            }
            requested_mode = static_cast<u8>(parsed.value());
        }
        // Firmware reports an 80-wire cable in bit 4, as Linux's pata_via
        // reads it, but it selected no UDMA mode on this Pegasos board. Do
        // not jump straight from PIO to the drive's fastest advertised mode.
        auto selected_mode = choose_via_udma_mode(identity.supported_udma_modes, identity.selected_udma_modes,
            (old_timing & via_udma_cable_bit) != 0, requested_mode);
        if (!selected_mode.has_value()) {
            dmesgln("IDE: {} drive has no usable Ultra DMA mode, using PIO", drive_name);
            return;
        }
        mode = selected_mode.value();
        u8 new_timing = (old_timing & via_udma_cable_bit) | via_udma_enable_bits | via_udma100_cycle_fields[mode];
        write_config8(identifier, offset, new_timing);
        dmesgln("IDE: {} drive Ultra DMA timing register {:#02x}: {:#02x} -> {:#02x}", drive_name, offset, old_timing, new_timing);
    } else {
        // Without chipset knowledge, keep the mode and controller timing the firmware set up.
        if (!channel.bus_master_reports_dma_capable(drive) || identity.selected_udma_modes == 0) {
            dmesgln("IDE: {} drive was not set up for DMA by the firmware, using PIO", drive_name);
            return;
        }
        mode = highest_mode(identity.selected_udma_modes);
    }
    if (auto result = channel.enable_udma(drive, mode); result.is_error()) {
        dmesgln("IDE: {} drive rejected Ultra DMA mode {}: {}, using PIO", drive_name, mode, result.error());
        return;
    }
    dmesgln("IDE: {} drive using Ultra DMA mode {}", drive_name, mode);
}

LockRefPtr<StorageDevice> IDEController::device(u32 index) const
{
    if (index >= m_devices.size())
        return {};
    return *m_devices[index];
}

}
