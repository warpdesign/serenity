/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Bus/PCI/AddressAssignment.h>

extern "C" int pci_address_assignment_test()
{
    using Kernel::PCI::address_range_fits;
    using Kernel::PCI::address_in_window;
    using Kernel::PCI::align_bus_address;

    auto aligned = align_bus_address(0x1'0000'1001ull, 0x2000);
    if (!aligned.has_value() || aligned.value() != 0x1'0000'2000ull)
        return 0;
    auto large_alignment = align_bus_address(0x1'0000'1000ull, 0x1'0000'0000ull);
    if (!large_alignment.has_value() || large_alignment.value() != 0x2'0000'0000ull)
        return 0;
    if (align_bus_address(~0ull, 0x1000).has_value()
        || align_bus_address(1, 0).has_value()
        || align_bus_address(1, 3).has_value())
        return 0;
    return address_range_fits(0x1'0000'2000ull, 0x3000, 0x1'0000'5000ull)
        && !address_range_fits(0x1'0000'2000ull, 0x3001, 0x1'0000'5000ull)
        && !address_range_fits(~0ull - 1, 4, ~0ull)
        && address_in_window(~0ull, ~0ull - 3, 4)
        && !address_in_window(0, ~0ull - 3, 4);
}
