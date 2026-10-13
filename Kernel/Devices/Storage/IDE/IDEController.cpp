/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Bus/PCI/API.h>
#include <Kernel/Devices/Storage/IDE/IDEChannel.h>
#include <Kernel/Devices/Storage/IDE/IDEController.h>
#include <Kernel/Devices/Storage/IDE/IDEDevice.h>
#include <Kernel/Devices/Storage/StorageManagement.h>
#include <Kernel/Library/IOWindow.h>

namespace Kernel {

// PCI IDE Controller Specification 1.0, programming interface byte.
static constexpr u8 channel_native_mode(u8 channel_index) { return 1 << (channel_index * 2); }
static constexpr u8 channel_mode_switchable(u8 channel_index) { return 1 << (channel_index * 2 + 1); }

bool IDEController::is_handled(PCI::DeviceIdentifier const& identifier)
{
    return identifier.class_code() == PCI::ClassID::MassStorage
        && identifier.subclass_code() == to_underlying(PCI::MassStorage::SubclassID::IDEController);
}

ErrorOr<NonnullRefPtr<IDEController>> IDEController::initialize(PCI::DeviceIdentifier const& identifier)
{
    auto controller = TRY(adopt_nonnull_ref_or_enomem(new (nothrow) IDEController()));
    PCI::enable_io_space(identifier);
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
    auto channel = TRY(IDEChannel::create(move(command_block), move(control_block)));

    for (auto drive : { IDEChannel::Drive::Master, IDEChannel::Drive::Slave }) {
        auto identity = TRY(channel->identify(drive));
        if (!identity.has_value())
            continue;
        StorageDevice::LUNAddress lun { controller_id(), channel_index, to_underlying(drive) };
        auto device = TRY(IDEDevice::create(channel, drive, identity.value(), lun, hardware_relative_controller_id()));
        TRY(m_devices.try_append(move(device)));
    }
    return {};
}

LockRefPtr<StorageDevice> IDEController::device(u32 index) const
{
    if (index >= m_devices.size())
        return {};
    return *m_devices[index];
}

}
