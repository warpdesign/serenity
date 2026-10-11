/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/Pegasos2/PCIConfig.h>

namespace Kernel::PowerPC::Pegasos2 {

Optional<u32> pci_config_address(u8 bus, u8 device, u8 function, u16 offset)
{
    if (device >= 32 || function >= 8 || offset > 0xfc || (offset & 3u) != 0)
        return {};
    return 0x80000000u | (static_cast<u32>(bus) << 16) | (static_cast<u32>(device) << 11)
        | (static_cast<u32>(function) << 8) | offset;
}

Optional<u32> read_pci_config32(volatile u32* address_register, volatile u32* data_register,
    u8 bus, u8 device, u8 function, u16 offset)
{
    if (!address_register || !data_register)
        return {};
    auto address = pci_config_address(bus, device, function, offset);
    if (!address.has_value())
        return {};

    // MV64361 configuration registers use little-endian MMIO on a big-endian
    // PowerPC CPU. Order the address selection before reading the data port.
    asm volatile("stwbrx %0,0,%1; eieio" : : "r"(address.value()), "r"(address_register) : "memory");
    u32 value;
    asm volatile("lwbrx %0,0,%1" : "=r"(value) : "r"(data_register) : "memory");
    return value;
}

}
