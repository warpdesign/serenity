/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/Pegasos2/PCIConfig.h>
#include <Kernel/Arch/powerpc/Pegasos2/PCIHostController.h>
#include <Kernel/Bus/PCI/AddressAssignment.h>
#include <Kernel/Locking/Spinlock.h>
#include <Kernel/Memory/MemoryManager.h>

namespace Kernel::PowerPC::Pegasos2 {

static Spinlock<LockRank::None> s_rtas_config_lock;

ErrorOr<NonnullOwnPtr<PCIHostController>> PCIHostController::create(PhysicalAddress registers_base,
    PowerPCPCIHostBridgeInfo const& info, Optional<PowerPCRtasInfo> const& rtas)
{
    if (info.bridge_number >= POWERPC_PEGASOS2_PCI_HOST_BRIDGE_COUNT
        || info.start_bus > info.end_bus || info.end_bus > 255
        || info.range_count == 0 || info.range_count > POWERPC_PEGASOS2_PCI_MAX_RANGES)
        return EINVAL;
    auto offsets = pci_config_register_offsets(info.bridge_number);
    if (!offsets.has_value())
        return EINVAL;
    auto registers = TRY(Memory::map_typed<volatile u32[]>(registers_base, 0x1000, Memory::Region::Access::ReadWrite));
    Memory::TypedMapping<volatile PowerPCRtasArguments> arguments;
    if (rtas.has_value())
        arguments = TRY(Memory::map_typed<volatile PowerPCRtasArguments>(
            PhysicalAddress { rtas->arguments_physical }, sizeof(PowerPCRtasArguments), Memory::Region::Access::ReadWrite));
    PCI::Domain domain { info.bridge_number, static_cast<u8>(info.start_bus), static_cast<u8>(info.end_bus) };
    auto controller = TRY(adopt_nonnull_own_or_enomem(new (nothrow) PCIHostController(
        domain, move(registers), offsets->address_offset, rtas, move(arguments))));
    for (u32 index = 0; index < info.range_count; ++index) {
        auto const& range = info.ranges[index];
        if (range.length == 0 || range.length > 0xffffffffull || range.host_address >= 0x100000000ull
            || range.length > 0x100000000ull - range.host_address || range.bus_address > ~0ull - range.length)
            return EINVAL;
        PCI::HostController::Window window { PhysicalAddress { range.host_address }, range.bus_address, static_cast<size_t>(range.length) };
        if (range.space == 1)
            TRY(controller->add_io_space_window(window));
        else if (range.space == 2 || range.space == 3)
            TRY(controller->add_memory_space_window(window));
        else
            return EINVAL;
    }
    return controller;
}

PCIHostController::PCIHostController(PCI::Domain const& domain, Memory::TypedMapping<volatile u32[]> registers,
    u16 address_offset, Optional<PowerPCRtasInfo> const& rtas, Memory::TypedMapping<volatile PowerPCRtasArguments> arguments)
    : HostController(domain)
    , m_registers(move(registers))
    , m_address_offset(address_offset)
    , m_rtas(rtas)
    , m_rtas_arguments(move(arguments))
{
}

Optional<u32> PCIHostController::read_rtas_config(PCI::BusNumber bus, PCI::DeviceNumber device,
    PCI::FunctionNumber function, u32 field, u32 size)
{
    if (!m_rtas.has_value() || field + size > 256 || (field & (size - 1)) != 0)
        return {};
    SpinlockLocker locker(s_rtas_config_lock);
    auto* arguments = m_rtas_arguments.ptr();
    arguments->token = m_rtas->read_pci_config_token;
    arguments->argument_count = 2;
    arguments->result_count = 2;
    arguments->values[0] = powerpc_rtas_pci_config_address(domain_number(), bus.value(),
        device.value(), function.value(), field);
    arguments->values[1] = size;
    arguments->values[2] = 0xffffffffu;
    arguments->values[3] = 0xffffffffu;
    asm volatile("sync" : : : "memory");
    auto real_return = Memory::virtual_to_low_physical(reinterpret_cast<FlatPtr>(powerpc_rtas_return_real));
    powerpc_rtas_enter(m_rtas->arguments_physical, m_rtas->entry_physical,
        real_return, reinterpret_cast<FlatPtr>(powerpc_rtas_return_virtual));
    asm volatile("sync" : : : "memory");
    if (arguments->values[2] != 0)
        return {};
    return arguments->values[3];
}

bool PCIHostController::write_rtas_config(PCI::BusNumber bus, PCI::DeviceNumber device,
    PCI::FunctionNumber function, u32 field, u32 size, u32 value)
{
    if (!m_rtas.has_value() || field + size > 256 || (field & (size - 1)) != 0)
        return false;
    SpinlockLocker locker(s_rtas_config_lock);
    auto* arguments = m_rtas_arguments.ptr();
    arguments->token = m_rtas->write_pci_config_token;
    arguments->argument_count = 3;
    arguments->result_count = 1;
    arguments->values[0] = powerpc_rtas_pci_config_address(domain_number(), bus.value(),
        device.value(), function.value(), field);
    arguments->values[1] = size;
    arguments->values[2] = value;
    arguments->values[3] = 0xffffffffu;
    asm volatile("sync" : : : "memory");
    auto real_return = Memory::virtual_to_low_physical(reinterpret_cast<FlatPtr>(powerpc_rtas_return_real));
    powerpc_rtas_enter(m_rtas->arguments_physical, m_rtas->entry_physical,
        real_return, reinterpret_cast<FlatPtr>(powerpc_rtas_return_virtual));
    asm volatile("sync" : : : "memory");
    return arguments->values[3] == 0;
}

ErrorOr<void> PCIHostController::assign_unconfigured_resources(PowerPCPCIHostBridgeInfo const& info)
{
    if (info.bridge_number != domain_number() || info.start_bus > info.end_bus || info.end_bus > 255
        || info.range_count == 0 || info.range_count > POWERPC_PEGASOS2_PCI_MAX_RANGES)
        return EINVAL;

    u64 next_address[POWERPC_PEGASOS2_PCI_MAX_RANGES] {};
    for (u32 index = 0; index < info.range_count; ++index) {
        auto const& range = info.ranges[index];
        if (range.length == 0 || range.bus_address > ~0ull - range.length)
            return EINVAL;
        next_address[index] = range.bus_address;
        // The low I/O ports belong to the legacy ISA devices on Pegasos 2.
        // A zero BAR address also cannot describe an assigned PCI resource.
        if (next_address[index] < 0x1000)
            next_address[index] = 0x1000;
    }

    // First keep firmware-assigned BARs out of the allocator's address space.
    // Then size and assign unconfigured BARs before the generic PCI drivers run.
    bool allocation_failed = false;
    for (u32 pass = 0; pass < 2; ++pass) {
        for (u32 bus = info.start_bus; bus <= info.end_bus; ++bus) {
            for (u32 device = 0; device < 32; ++device) {
                if (read16_field(bus, device, 0, 0) == 0xffffu)
                    continue;
                u32 function_count = (read8_field(bus, device, 0, 0x0e) & 0x80u) ? 8 : 1;
                for (u32 function = 0; function < function_count; ++function) {
                    u16 vendor = read16_field(bus, device, function, 0);
                    if (vendor == 0 || vendor == 0xffffu)
                        continue;
                    u32 header_type = read8_field(bus, device, function, 0x0e) & 0x7fu;
                    u32 bar_count = header_type == 0 ? 6 : header_type == 1 ? 2 : 0;
                    u16 command = read16_field(bus, device, function, 0x04);
                    write16_field(bus, device, function, 0x04, command & ~3u);
                    u16 enabled_spaces = 0;
                    for (u32 bar = 0; bar < bar_count; ++bar) {
                        u32 offset = 0x10 + bar * 4;
                        u32 original = read32_field(bus, device, function, offset);
                        bool is_io = (original & 1u) != 0;
                        bool is_64_bit = !is_io && (original & 6u) == 4u;
                        if (is_64_bit && bar + 1 == bar_count)
                            break;
                        u32 original_high = is_64_bit ? read32_field(bus, device, function, offset + 4) : 0;
                        write32_field(bus, device, function, offset, 0xffffffffu);
                        if (is_64_bit)
                            write32_field(bus, device, function, offset + 4, 0xffffffffu);
                        u32 mask_low = read32_field(bus, device, function, offset);
                        u32 mask_high = is_64_bit ? read32_field(bus, device, function, offset + 4) : 0;
                        if (is_64_bit)
                            write32_field(bus, device, function, offset + 4, original_high);
                        write32_field(bus, device, function, offset, original);

                        u64 mask = is_io ? static_cast<u64>(mask_low & ~3u)
                            : (static_cast<u64>(mask_high) << 32) | (mask_low & ~15u);
                        if (is_io || !is_64_bit)
                            mask |= 0xffffffff00000000ull;
                        u64 size = ~mask + 1;
                        if (mask_low == 0 || mask_low == 0xffffffffu || size == 0 || (size & (size - 1)) != 0) {
                            if (is_64_bit)
                                ++bar;
                            continue;
                        }

                        u64 assigned = is_io ? original & ~3u : original & ~15u;
                        if (is_64_bit)
                            assigned |= static_cast<u64>(original_high) << 32;
                        bool valid_assignment = false;
                        for (u32 range_index = 0; range_index < info.range_count; ++range_index) {
                            auto const& range = info.ranges[range_index];
                            if (is_io ? range.space != 1 : range.space == 1)
                                continue;
                            auto range_end = range.bus_address + range.length;
                            if (pass == 0 && assigned != 0 && PCI::address_in_window(assigned, range.bus_address, range.length)
                                && PCI::address_range_fits(assigned, size, range_end)) {
                                auto end = assigned + size;
                                if (end > next_address[range_index])
                                    next_address[range_index] = end;
                                valid_assignment = true;
                            } else if (pass == 1 && assigned == 0) {
                                auto aligned = PCI::align_bus_address(next_address[range_index], size);
                                if (!aligned.has_value() || !PCI::address_range_fits(*aligned, size, range_end))
                                    continue;
                                assigned = *aligned;
                                next_address[range_index] = assigned + size;
                                write32_field(bus, device, function, offset,
                                    static_cast<u32>(assigned) | (original & (is_io ? 3u : 15u)));
                                if (is_64_bit)
                                    write32_field(bus, device, function, offset + 4, static_cast<u32>(assigned >> 32));
                                valid_assignment = true;
                            } else if (pass == 1 && assigned != 0
                                && PCI::address_in_window(assigned, range.bus_address, range.length)
                                && PCI::address_range_fits(assigned, size, range_end)) {
                                valid_assignment = true;
                            }
                            if (valid_assignment)
                                break;
                        }
                        if (valid_assignment)
                            enabled_spaces |= is_io ? 1u : 2u;
                        else if (pass == 1)
                            allocation_failed = true;
                        if (is_64_bit)
                            ++bar;
                    }
                    write16_field(bus, device, function, 0x04, command | enabled_spaces);
                }
            }
        }
    }
    if (allocation_failed)
        return ENOSPC;
    return {};
}

size_t PCIHostController::assign_unconfigured_interrupt_lines(PowerPCPCIHostBridgeInfo const& info, Array<u8, 4> const& pirq_lines)
{
    // The MV64361 root buses rotate INTA-D by the device slot before sending
    // them to the VT8231 PIRQ inputs. PCI devices behind a bridge need another
    // swizzle and are not covered here.
    size_t assigned = 0;
    auto bus = info.start_bus;
    for (u32 device = 0; device < 32; ++device) {
        if (read16_field(bus, device, 0, 0) == 0xffffu)
            continue;
        u32 function_count = (read8_field(bus, device, 0, 0x0e) & 0x80u) ? 8 : 1;
        for (u32 function = 0; function < function_count; ++function) {
            u16 vendor = read16_field(bus, device, function, 0);
            if (vendor == 0 || vendor == 0xffffu)
                continue;
            u8 line = read8_field(bus, device, function, 0x3c);
            u8 pin = read8_field(bus, device, function, 0x3d);
            if ((line != 0 && line != 0xff) || pin < 1 || pin > 4)
                continue;
            u8 routed_line = pirq_lines[(device + pin - 1) % pirq_lines.size()];
            if (routed_line == 0 || routed_line == 2 || routed_line >= 16)
                continue;
            write8_field(bus, device, function, 0x3c, routed_line);
            ++assigned;
        }
    }
    return assigned;
}

u8 PCIHostController::read8_field_locked(PCI::BusNumber bus, PCI::DeviceNumber device, PCI::FunctionNumber function, u32 field)
{
    VERIFY(m_access_lock.is_locked());
    if (m_rtas.has_value())
        return read_rtas_config(bus, device, function, field, 1).value_or(0xffu);
    if (field > 0xff)
        return 0xff;
    return read_pci_config8(address_register(), data_register(), bus.value(), device.value(), function.value(), static_cast<u16>(field)).value_or(0xff);
}

u16 PCIHostController::read16_field_locked(PCI::BusNumber bus, PCI::DeviceNumber device, PCI::FunctionNumber function, u32 field)
{
    VERIFY(m_access_lock.is_locked());
    if (m_rtas.has_value())
        return read_rtas_config(bus, device, function, field, 2).value_or(0xffffu);
    if (field > 0xff)
        return 0xffff;
    return read_pci_config16(address_register(), data_register(), bus.value(), device.value(), function.value(), static_cast<u16>(field)).value_or(0xffff);
}

u32 PCIHostController::read32_field_locked(PCI::BusNumber bus, PCI::DeviceNumber device, PCI::FunctionNumber function, u32 field)
{
    VERIFY(m_access_lock.is_locked());
    if (m_rtas.has_value())
        return read_rtas_config(bus, device, function, field, 4).value_or(0xffffffffu);
    if (field > 0xff)
        return 0xffffffffu;
    return read_pci_config32(address_register(), data_register(), bus.value(), device.value(), function.value(), static_cast<u16>(field)).value_or(0xffffffffu);
}

void PCIHostController::write8_field_locked(PCI::BusNumber bus, PCI::DeviceNumber device, PCI::FunctionNumber function, u32 field, u8 value)
{
    VERIFY(m_access_lock.is_locked());
    if (m_rtas.has_value()) {
        (void)write_rtas_config(bus, device, function, field, 1, value);
        return;
    }
    if (field <= 0xff)
        (void)write_pci_config8(address_register(), data_register(), bus.value(), device.value(), function.value(), static_cast<u16>(field), value);
}

void PCIHostController::write16_field_locked(PCI::BusNumber bus, PCI::DeviceNumber device, PCI::FunctionNumber function, u32 field, u16 value)
{
    VERIFY(m_access_lock.is_locked());
    if (m_rtas.has_value()) {
        (void)write_rtas_config(bus, device, function, field, 2, value);
        return;
    }
    if (field <= 0xff)
        (void)write_pci_config16(address_register(), data_register(), bus.value(), device.value(), function.value(), static_cast<u16>(field), value);
}

void PCIHostController::write32_field_locked(PCI::BusNumber bus, PCI::DeviceNumber device, PCI::FunctionNumber function, u32 field, u32 value)
{
    VERIFY(m_access_lock.is_locked());
    if (m_rtas.has_value()) {
        (void)write_rtas_config(bus, device, function, field, 4, value);
        return;
    }
    if (field <= 0xff)
        (void)write_pci_config32(address_register(), data_register(), bus.value(), device.value(), function.value(), static_cast<u16>(field), value);
}

}
