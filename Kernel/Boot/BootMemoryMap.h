/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

// A firmware loader translates its memory properties into these ranges before
// entering the kernel. The pointed-to arrays must remain mapped through early
// physical memory initialization.
struct BootMemoryRange {
    unsigned long long physical_start;
    unsigned long long length;
};

struct BootMemoryMap {
    struct BootMemoryRange const* usable_ranges;
    unsigned int usable_range_count;
    struct BootMemoryRange const* reserved_ranges;
    unsigned int reserved_range_count;
};
