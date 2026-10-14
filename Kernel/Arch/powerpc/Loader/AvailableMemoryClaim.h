/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>
#include <Kernel/Boot/BootMemoryMap.h>

namespace Kernel::PowerPC::Loader {

// Prefer the largest available range above the loader, excluding earlier claims.
inline bool choose_available_claim_address(BootMemoryRange const* ranges, u32 range_count,
    BootMemoryRange const* claimed, u32 claimed_count, u32 loader_end,
    u32 size, u32 alignment, u32& address)
{
    if (!ranges || range_count == 0 || (claimed_count != 0 && !claimed) || size == 0
        || alignment == 0 || (alignment & (alignment - 1)) != 0)
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
        for (;;) {
            if (candidate == 0 || candidate > 0xffffffffu || candidate + size > end)
                break;
            bool overlaps_claim = false;
            for (u32 claim_index = 0; claim_index < claimed_count; ++claim_index) {
                u64 claim_start = claimed[claim_index].physical_start;
                u64 claim_end = claim_start + claimed[claim_index].length;
                if (claim_end < claim_start || claim_end > 0x100000000ull)
                    return false;
                if (candidate < claim_end && claim_start < candidate + size) {
                    candidate = (claim_end + alignment - 1) & ~static_cast<u64>(alignment - 1);
                    overlaps_claim = true;
                    break;
                }
            }
            if (!overlaps_claim)
                break;
        }
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
