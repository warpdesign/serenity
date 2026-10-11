/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>

namespace Kernel::PowerPC {

constexpr u32 default_decrementer_interrupts_per_second = 250;

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

inline u64 timebase_ticks_for_microseconds(u32 microseconds, u32 timebase_frequency)
{
    auto product = static_cast<u64>(microseconds) * timebase_frequency;
    return product / 1'000'000ull + (product % 1'000'000ull != 0);
}

inline void delay_timebase_ticks(u64 ticks)
{
    auto start = read_timebase();
    while (read_timebase() - start < ticks)
        asm volatile("nop" : : : "memory");
}

inline u64 timebase_ticks_to_nanoseconds(u64 ticks, u32 timebase_frequency)
{
    if (timebase_frequency == 0)
        return 0;
    constexpr u64 nanoseconds_per_second = 1'000'000'000ull;
    constexpr u64 maximum = ~0ull;
    auto whole_seconds = ticks / timebase_frequency;
    if (whole_seconds > maximum / nanoseconds_per_second)
        return maximum;
    auto nanoseconds = whole_seconds * nanoseconds_per_second;
    auto fraction = (ticks % timebase_frequency) * nanoseconds_per_second / timebase_frequency;
    return fraction > maximum - nanoseconds ? maximum : nanoseconds + fraction;
}

}
