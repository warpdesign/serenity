/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>

namespace Kernel::PowerPC::Pegasos2 {

// Only for the real-mode boot probe. The kernel maps the PCI I/O window itself.
// The MV64361 exposes the VT8231 ISA I/O ports through this PCI I/O window.
constexpr FlatPtr legacy_io_base = 0xfe000000u;
constexpr FlatPtr master_pic_mask_port = 0x21u;
constexpr FlatPtr slave_pic_mask_port = 0xa1u;

inline u8 read_legacy_port(FlatPtr port)
{
    auto value = *reinterpret_cast<volatile u8 const*>(legacy_io_base + port);
    asm volatile("eieio" : : : "memory");
    return value;
}

inline void write_legacy_port(FlatPtr port, u8 value)
{
    *reinterpret_cast<volatile u8*>(legacy_io_base + port) = value;
    asm volatile("eieio" : : : "memory");
}

}
