/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int PowerPCVSIDWord;

enum {
    POWERPC_VSID_BLOCK_SIZE = 16,
    POWERPC_FIRST_USER_VSID_BASE = 0x100,
    POWERPC_LAST_USER_VSID_BASE = 0x00fffff0,
};

struct PowerPCVSIDAllocator {
    PowerPCVSIDWord next_base;
};

void powerpc_vsid_allocator_initialize(struct PowerPCVSIDAllocator*);
// Returns zero when all user VSIDs have been allocated. Bases are never
// reused until the HPT can evict every mapping belonging to an address space.
PowerPCVSIDWord powerpc_vsid_allocator_allocate(struct PowerPCVSIDAllocator*);

#ifdef __cplusplus
}
#endif
