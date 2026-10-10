/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Optional.h>
#include <AK/Types.h>

namespace Kernel::PCI {

// PCI bus addresses can exceed the native pointer width on a 32-bit kernel.
constexpr Optional<u64> align_bus_address(u64 address, u64 alignment)
{
    if (alignment == 0 || (alignment & (alignment - 1)) != 0)
        return {};
    auto mask = alignment - 1;
    if (address > ~0ull - mask)
        return {};
    return (address + mask) & ~mask;
}

constexpr bool address_range_fits(u64 address, u64 length, u64 end)
{
    return address <= end && length <= end - address;
}

constexpr bool address_in_window(u64 address, u64 start, u64 length)
{
    return address >= start && address - start < length;
}

}
