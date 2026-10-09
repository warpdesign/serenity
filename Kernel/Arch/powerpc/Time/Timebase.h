/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>

namespace Kernel::PowerPC {

inline u64 read_timebase()
{
    u32 high_before;
    u32 low;
    u32 high_after;
    do {
        asm volatile("mftbu %0; mftb %1; mftbu %2" : "=r"(high_before), "=r"(low), "=r"(high_after));
    } while (high_before != high_after);
    return (static_cast<u64>(high_before) << 32) | low;
}

inline void set_decrementer(u32 ticks)
{
    asm volatile("mtdec %0" : : "r"(ticks) : "memory");
}

inline u32 decrementer_interval(u32 timebase_frequency, u32 interrupts_per_second)
{
    if (timebase_frequency == 0 || interrupts_per_second == 0)
        return 0;
    auto ticks = timebase_frequency / interrupts_per_second;
    // A positive signed DEC value is needed for a future zero crossing.
    return ticks > 0 && ticks <= 0x7fffffff ? ticks : 0;
}

}
