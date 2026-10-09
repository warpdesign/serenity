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

}
