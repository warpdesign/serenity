/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/MemoryFences.h>

extern "C" int powerpc_memory_fences_test()
{
    volatile unsigned descriptor[2] { 0, 0 };
    volatile unsigned doorbell = 0;

    descriptor[0] = 0x12345678;
    descriptor[1] = 0x87654321;
    Kernel::store_memory_fence();
    doorbell = 1;

    Kernel::load_memory_fence();
    if (doorbell != 1 || descriptor[0] != 0x12345678 || descriptor[1] != 0x87654321)
        return 0;

    descriptor[0] = 0xabcdef01;
    Kernel::full_memory_fence();
    return descriptor[0] == 0xabcdef01;
}
