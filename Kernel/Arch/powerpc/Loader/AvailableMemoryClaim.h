/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>
#include <Kernel/Boot/BootMemoryMap.h>

namespace Kernel::PowerPC::Loader {

// Prefer the largest available range above the loader, and leave the low firmware area alone.
inline bool choose_claim_probe_address(BootMemoryRange const* ranges, u32 range_count, u32 loader_end,
    u32 size, u32 alignment, u32& address)
{
    if (!ranges || range_count == 0 || size == 0 || alignment == 0 || (alignment & (alignment - 1)) != 0)
        return false;

    u64 best_usable_size = 0;
    for (u32 index = 0; index < range_count; ++index) {
        u64 start = ranges[index].physical_start;
        u64 end = start + ranges[index].length;
        if (ranges[index].length == 0 || end > 0x100000000ull || end < start)
            continue;
        if (start < loader_end)
            start = loader_end;
        u64 candidate = (start + alignment - 1) & ~static_cast<u64>(alignment - 1);
        if (candidate == 0 || candidate > 0xffffffffu || candidate + size > end)
            continue;
        u64 usable_size = end - candidate;
        if (usable_size > best_usable_size) {
            best_usable_size = usable_size;
            address = static_cast<u32>(candidate);
        }
    }
    return best_usable_size != 0;
}

}
