/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/VSIDAllocator.h>

extern "C" int powerpc_vsid_allocator_test()
{
    PowerPCVSIDAllocator allocator {};
    powerpc_vsid_allocator_initialize(&allocator);
    auto first = powerpc_vsid_allocator_allocate(&allocator);
    auto second = powerpc_vsid_allocator_allocate(&allocator);
    if (first != 0x100u || second != 0x110u || second - first != 16u)
        return 0;

    allocator.next_base = 0x00fffff0u;
    auto last = powerpc_vsid_allocator_allocate(&allocator);
    return last == 0x00fffff0u && powerpc_vsid_allocator_allocate(&allocator) == 0
        && allocator.next_base == 0x01000000u;
}
