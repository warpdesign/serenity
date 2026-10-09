/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int PowerPCHPTWord;

enum {
    POWERPC_HPT_PAGE_SIZE = 4096,
    POWERPC_HPT_ENTRIES_PER_GROUP = 8,
    POWERPC_HPT_VALID = 0x80000000u,
    POWERPC_HPT_SECONDARY_HASH = 0x40,
    POWERPC_HPT_REFERENCED = 0x100,
    POWERPC_HPT_CHANGED = 0x80,
    POWERPC_HPT_COHERENT = 0x10,
    POWERPC_HPT_CACHE_INHIBITED = 0x20,
    POWERPC_HPT_GUARDED = 0x08,
    POWERPC_HPT_SUPERVISOR_ONLY = 0x0,
    POWERPC_HPT_READ_WRITE = 0x2,
    POWERPC_HPT_READ_ONLY = 0x3,
};

struct PowerPCHPTEntry {
    volatile PowerPCHPTWord upper;
    volatile PowerPCHPTWord lower;
};

// The caller provides a power-of-two number of 64-byte groups and serializes
// changes. The entry array must be mapped while these routines execute.
struct PowerPCHashPageTable {
    struct PowerPCHPTEntry* entries;
    PowerPCHPTWord group_count;
};

struct PowerPCHPTEntry* powerpc_hpt_find(struct PowerPCHashPageTable const*, PowerPCHPTWord vsid, PowerPCHPTWord effective_address);
int powerpc_hpt_map(struct PowerPCHashPageTable const*, PowerPCHPTWord vsid, PowerPCHPTWord effective_address, PowerPCHPTWord physical_address, PowerPCHPTWord lower_flags);
int powerpc_hpt_remap(struct PowerPCHashPageTable const*, PowerPCHPTWord vsid, PowerPCHPTWord effective_address, PowerPCHPTWord physical_address, PowerPCHPTWord lower_flags);
int powerpc_hpt_unmap(struct PowerPCHashPageTable const*, PowerPCHPTWord vsid, PowerPCHPTWord effective_address);
int powerpc_hpt_purge_vsid_block(struct PowerPCHashPageTable const*, PowerPCHPTWord first_vsid, PowerPCHPTWord vsid_count);

#ifdef __cplusplus
}
#endif
