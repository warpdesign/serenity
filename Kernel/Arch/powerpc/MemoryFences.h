/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Platform.h>
VALIDATE_IS_POWERPC32()

namespace Kernel {

// These fences also order cached RAM against device accesses. eieio only
// orders accesses to cache-inhibited or write-through storage on the 750.
ALWAYS_INLINE void full_memory_fence()
{
    asm volatile("sync" ::: "memory");
}

ALWAYS_INLINE void load_memory_fence()
{
    asm volatile("sync" ::: "memory");
}

ALWAYS_INLINE void store_memory_fence()
{
    asm volatile("sync" ::: "memory");
}

}
