/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/Pegasos2/PCIConfig.h>
#include <Kernel/Arch/powerpc/Pegasos2/PCIHostController.h>

namespace Kernel::PowerPC::Pegasos2 {

ErrorOr<NonnullOwnPtr<PCIHostController>> PCIHostController::create(PhysicalAddress registers_base, PowerPCPCIHostBridgeInfo const& info)
{
    if (info.bridge_number >= POWERPC_PEGASOS2_PCI_HOST_BRIDGE_COUNT
        || info.start_bus > info.end_bus || info.end_bus > 255
        || info.range_count == 0 || info.range_count > POWERPC_PEGASOS2_PCI_MAX_RANGES)
        return EINVAL;
    auto offsets = pci_config_register_offsets(info.bridge_number);
    if (!offsets.has_value())
        return EINVAL;
    auto registers = TRY(Memory::map_typed<volatile u32[]>(registers_base, 0x1000, Memory::Region::Access::ReadWrite));
    PCI::Domain domain { info.bridge_number, static_cast<u8>(info.start_bus), static_cast<u8>(info.end_bus) };
    auto controller = TRY(adopt_nonnull_own_or_enomem(new (nothrow) PCIHostController(domain, move(registers), offsets->address_offset)));
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

PCIHostController::PCIHostController(PCI::Domain const& domain, Memory::TypedMapping<volatile u32[]> registers, u16 address_offset)
    : HostController(domain)
    , m_registers(move(registers))
    , m_address_offset(address_offset)
{
}

u8 PCIHostController::read8_field_locked(PCI::BusNumber bus, PCI::DeviceNumber device, PCI::FunctionNumber function, u32 field)
{
    VERIFY(m_access_lock.is_locked());
    if (field > 0xff)
        return 0xff;
    return read_pci_config8(address_register(), data_register(), bus.value(), device.value(), function.value(), static_cast<u16>(field)).value_or(0xff);
}

u16 PCIHostController::read16_field_locked(PCI::BusNumber bus, PCI::DeviceNumber device, PCI::FunctionNumber function, u32 field)
{
    VERIFY(m_access_lock.is_locked());
    if (field > 0xff)
        return 0xffff;
    return read_pci_config16(address_register(), data_register(), bus.value(), device.value(), function.value(), static_cast<u16>(field)).value_or(0xffff);
}

u32 PCIHostController::read32_field_locked(PCI::BusNumber bus, PCI::DeviceNumber device, PCI::FunctionNumber function, u32 field)
{
    VERIFY(m_access_lock.is_locked());
    if (field > 0xff)
        return 0xffffffffu;
    return read_pci_config32(address_register(), data_register(), bus.value(), device.value(), function.value(), static_cast<u16>(field)).value_or(0xffffffffu);
}

void PCIHostController::write8_field_locked(PCI::BusNumber bus, PCI::DeviceNumber device, PCI::FunctionNumber function, u32 field, u8 value)
{
    VERIFY(m_access_lock.is_locked());
    if (field <= 0xff)
        (void)write_pci_config8(address_register(), data_register(), bus.value(), device.value(), function.value(), static_cast<u16>(field), value);
}

void PCIHostController::write16_field_locked(PCI::BusNumber bus, PCI::DeviceNumber device, PCI::FunctionNumber function, u32 field, u16 value)
{
    VERIFY(m_access_lock.is_locked());
    if (field <= 0xff)
        (void)write_pci_config16(address_register(), data_register(), bus.value(), device.value(), function.value(), static_cast<u16>(field), value);
}

void PCIHostController::write32_field_locked(PCI::BusNumber bus, PCI::DeviceNumber device, PCI::FunctionNumber function, u32 field, u32 value)
{
    VERIFY(m_access_lock.is_locked());
    if (field <= 0xff)
        (void)write_pci_config32(address_register(), data_register(), bus.value(), device.value(), function.value(), static_cast<u16>(field), value);
}

}
