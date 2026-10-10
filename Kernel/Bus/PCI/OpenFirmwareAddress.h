/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Endian.h>
#include <AK/StdLibExtraDetails.h>
#include <AK/Types.h>

namespace Kernel::PCI {

// PCI Bus Binding to Open Firmware, 2.2.1.1: phys.hi, phys.mid, phys.lo.
// Decode the cells explicitly: native bitfield allocation order differs
// between little- and big-endian target compilers.
struct OpenFirmwareAddress {
    enum class SpaceType : u32 {
        ConfigurationSpace = 0,
        IOSpace = 1,
        Memory32BitSpace = 2,
        Memory64BitSpace = 3,
    };

    BigEndian<u32> phys_hi;
    BigEndian<u64> io_or_memory_space_address;

    constexpr SpaceType space_type() const { return static_cast<SpaceType>((static_cast<u32>(phys_hi) >> 24) & 3u); }
    constexpr bool aliased() const { return (static_cast<u32>(phys_hi) & 0x20000000u) != 0; }
    constexpr bool prefetchable() const { return (static_cast<u32>(phys_hi) & 0x40000000u) != 0; }
    constexpr bool non_relocatable() const { return (static_cast<u32>(phys_hi) & 0x80000000u) != 0; }
    constexpr u8 bus() const { return (static_cast<u32>(phys_hi) >> 16) & 0xffu; }
    constexpr u8 device() const { return (static_cast<u32>(phys_hi) >> 11) & 0x1fu; }
    constexpr u8 function() const { return (static_cast<u32>(phys_hi) >> 8) & 7u; }
    constexpr u8 register_() const { return static_cast<u32>(phys_hi) & 0xffu; }

    constexpr OpenFirmwareAddress operator&(OpenFirmwareAddress const& other) const
    {
        return OpenFirmwareAddress {
            .phys_hi = static_cast<u32>(phys_hi) & static_cast<u32>(other.phys_hi),
            .io_or_memory_space_address = static_cast<u64>(io_or_memory_space_address) & static_cast<u64>(other.io_or_memory_space_address),
        };
    }

    constexpr OpenFirmwareAddress& operator&=(OpenFirmwareAddress const& other)
    {
        *this = *this & other;
        return *this;
    }
};
static_assert(AssertSize<OpenFirmwareAddress, 3 * sizeof(u32)>());

}
