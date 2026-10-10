/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Bus/PCI/OpenFirmwareAddress.h>

static_assert(sizeof(Kernel::PCI::OpenFirmwareAddress) == 12);

extern "C" int pci_open_firmware_address_test()
{
    using Kernel::PCI::OpenFirmwareAddress;
    // phys.hi = n,p,32-bit memory, bus 0x5a, device 3, function 3, BAR0;
    // phys.mid/lo is intentionally larger than 32 bits.
    u8 encoded[12] = { 0xc2, 0x5a, 0x1b, 0x10, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0 };
    OpenFirmwareAddress address {};
    __builtin_memcpy(&address, encoded, sizeof(address));

    if (address.space_type() != OpenFirmwareAddress::SpaceType::Memory32BitSpace
        || address.aliased() || !address.prefetchable() || !address.non_relocatable()
        || address.bus() != 0x5a || address.device() != 3 || address.function() != 3 || address.register_() != 0x10
        || address.io_or_memory_space_address != 0x123456789abcdef0ull)
        return 0;

    OpenFirmwareAddress all_bits { .phys_hi = 0xffffffffu, .io_or_memory_space_address = ~0ull };
    auto unchanged = address & all_bits;
    if (__builtin_memcmp(&unchanged, encoded, sizeof(unchanged)) != 0)
        return 0;

    OpenFirmwareAddress partial_mask { .phys_hi = 0x0000f800u, .io_or_memory_space_address = 0xff00ff00ff00ff00ull };
    address &= partial_mask;
    return address.phys_hi == 0x00001800u
        && address.io_or_memory_space_address == 0x120056009a00de00ull;
}
