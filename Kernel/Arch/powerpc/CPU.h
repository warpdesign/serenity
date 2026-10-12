/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Platform.h>
#include <AK/Types.h>

VALIDATE_IS_POWERPC32()

namespace Kernel::PowerPC {

constexpr u32 MSR_POW = 1u << 18;
constexpr u32 MSR_EE = 1u << 15;
constexpr u32 HID0_DOZE = 1u << 23;
constexpr u32 HID0_NAP = 1u << 22;
constexpr u32 HID0_SLEEP = 1u << 21;

inline u32 read_msr()
{
    u32 value;
    asm volatile("mfmsr %0" : "=r"(value));
    return value;
}

inline void write_msr(u32 value)
{
    asm volatile("mtmsr %0; isync" : : "r"(value) : "memory");
}

inline u32 read_hid0()
{
    u32 value;
    asm volatile("mfspr %0,1008" : "=r"(value));
    return value;
}

inline void write_hid0(u32 value)
{
    asm volatile("mtspr 1008,%0; isync" : : "r"(value) : "memory");
}

}
