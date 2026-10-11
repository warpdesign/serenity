/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/Pegasos2/PCIConfig.h>
#include <Kernel/Arch/powerpc/Pegasos2/HardwareAddresses.h>

namespace Kernel::PowerPC::Pegasos2 {

Optional<PCIConfigRegisters> pci_config_register_offsets(u32 bridge_number)
{
    if (bridge_number > 1)
        return {};
    u16 address = PEGASOS2_PCI_SOUTH_CONFIG_ADDRESS_OFFSET + bridge_number * 0x80u;
    return PCIConfigRegisters { address, static_cast<u16>(address + sizeof(u32)) };
}

Optional<u32> pci_config_address(u8 bus, u8 device, u8 function, u16 offset)
{
    if (device >= 32 || function >= 8 || offset > 0xfc || (offset & 3u) != 0)
        return {};
    return 0x80000000u | (static_cast<u32>(bus) << 16) | (static_cast<u32>(device) << 11)
        | (static_cast<u32>(function) << 8) | offset;
}

static bool select_config_field(volatile u32* address_register, volatile u32* data_register,
    u8 bus, u8 device, u8 function, u16 offset, u16 width)
{
    if (!address_register || !data_register)
        return false;
    if (offset >= 0x100 || offset + width > 0x100 || (offset & (width - 1)) != 0)
        return false;
    auto address = pci_config_address(bus, device, function, offset & ~3u);
    if (!address.has_value())
        return false;

    // MV64361 configuration registers use little-endian MMIO on a big-endian
    // PowerPC CPU. Order the address selection before accessing the data port.
    asm volatile("stwbrx %0,0,%1; eieio" : : "r"(address.value()), "r"(address_register) : "memory");
    return true;
}

Optional<u8> read_pci_config8(volatile u32* address_register, volatile u32* data_register,
    u8 bus, u8 device, u8 function, u16 offset)
{
    if (!select_config_field(address_register, data_register, bus, device, function, offset, 1))
        return {};
    auto* data = reinterpret_cast<volatile u8*>(data_register);
    return data[offset & 3u];
}

Optional<u16> read_pci_config16(volatile u32* address_register, volatile u32* data_register,
    u8 bus, u8 device, u8 function, u16 offset)
{
    if (!select_config_field(address_register, data_register, bus, device, function, offset, 2))
        return {};
    auto* field = reinterpret_cast<volatile u8*>(data_register) + (offset & 2u);
    u16 value;
    asm volatile("lhbrx %0,0,%1" : "=r"(value) : "r"(field) : "memory");
    return value;
}

Optional<u32> read_pci_config32(volatile u32* address_register, volatile u32* data_register,
    u8 bus, u8 device, u8 function, u16 offset)
{
    if (!select_config_field(address_register, data_register, bus, device, function, offset, 4))
        return {};
    u32 value;
    asm volatile("lwbrx %0,0,%1" : "=r"(value) : "r"(data_register) : "memory");
    return value;
}

bool write_pci_config8(volatile u32* address_register, volatile u32* data_register,
    u8 bus, u8 device, u8 function, u16 offset, u8 value)
{
    if (!select_config_field(address_register, data_register, bus, device, function, offset, 1))
        return false;
    auto* data = reinterpret_cast<volatile u8*>(data_register);
    data[offset & 3u] = value;
    asm volatile("eieio" : : : "memory");
    return true;
}

bool write_pci_config16(volatile u32* address_register, volatile u32* data_register,
    u8 bus, u8 device, u8 function, u16 offset, u16 value)
{
    if (!select_config_field(address_register, data_register, bus, device, function, offset, 2))
        return false;
    auto* field = reinterpret_cast<volatile u8*>(data_register) + (offset & 2u);
    asm volatile("sthbrx %0,0,%1; eieio" : : "r"(value), "r"(field) : "memory");
    return true;
}

bool write_pci_config32(volatile u32* address_register, volatile u32* data_register,
    u8 bus, u8 device, u8 function, u16 offset, u32 value)
{
    if (!select_config_field(address_register, data_register, bus, device, function, offset, 4))
        return false;
    asm volatile("stwbrx %0,0,%1; eieio" : : "r"(value), "r"(data_register) : "memory");
    return true;
}

}
