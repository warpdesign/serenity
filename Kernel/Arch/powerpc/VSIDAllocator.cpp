/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/VSIDAllocator.h>

static_assert(sizeof(PowerPCVSIDWord) == 4);

void powerpc_vsid_allocator_initialize(PowerPCVSIDAllocator* allocator)
{
    __atomic_store_n(&allocator->next_base, POWERPC_FIRST_USER_VSID_BASE, __ATOMIC_RELAXED);
}

PowerPCVSIDWord powerpc_vsid_allocator_allocate(PowerPCVSIDAllocator* allocator)
{
    if (!allocator)
        return 0;

    for (;;) {
        PowerPCVSIDWord current = __atomic_load_n(&allocator->next_base, __ATOMIC_RELAXED);
        if (current < POWERPC_FIRST_USER_VSID_BASE || current > POWERPC_LAST_USER_VSID_BASE
            || (current & (POWERPC_VSID_BLOCK_SIZE - 1)) != 0)
            return 0;

        PowerPCVSIDWord next = current + POWERPC_VSID_BLOCK_SIZE;
        if (__atomic_compare_exchange_n(&allocator->next_base, &current, next, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED))
            return current;
    }
}
