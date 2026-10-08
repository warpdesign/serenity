/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/HashPageTable.h>

_Static_assert(sizeof(PowerPCHPTWord) == 4, "PowerPC HPT word size");
_Static_assert(sizeof(struct PowerPCHPTEntry) == 8, "PowerPC HPT entry size");

static int valid_table(struct PowerPCHashPageTable const* table, PowerPCHPTWord vsid, PowerPCHPTWord effective_address)
{
    return table && table->entries && table->group_count != 0
        && (table->group_count & (table->group_count - 1)) == 0
        && (vsid & ~0x00ffffffu) == 0
        && (effective_address & (POWERPC_HPT_PAGE_SIZE - 1)) == 0;
}

static PowerPCHPTWord primary_hash(PowerPCHPTWord vsid, PowerPCHPTWord effective_address)
{
    return (vsid & 0x7ffffu) ^ ((effective_address >> 12) & 0xffffu);
}

static PowerPCHPTWord upper_word(PowerPCHPTWord vsid, PowerPCHPTWord effective_address, PowerPCHPTWord secondary)
{
    return POWERPC_HPT_VALID | (vsid << 7) | ((effective_address >> 22) & 0x3fu)
        | (secondary ? POWERPC_HPT_SECONDARY_HASH : 0);
}

static struct PowerPCHPTEntry* group_for(struct PowerPCHashPageTable const* table, PowerPCHPTWord hash)
{
    return &table->entries[(hash & (table->group_count - 1)) * POWERPC_HPT_ENTRIES_PER_GROUP];
}

struct PowerPCHPTEntry* powerpc_hpt_find(struct PowerPCHashPageTable const* table, PowerPCHPTWord vsid, PowerPCHPTWord effective_address)
{
    if (!valid_table(table, vsid, effective_address))
        return 0;

    PowerPCHPTWord hash = primary_hash(vsid, effective_address);
    for (PowerPCHPTWord secondary = 0; secondary < 2; ++secondary) {
        struct PowerPCHPTEntry* group = group_for(table, secondary ? ~hash : hash);
        PowerPCHPTWord expected = upper_word(vsid, effective_address, secondary);
        for (PowerPCHPTWord slot = 0; slot < POWERPC_HPT_ENTRIES_PER_GROUP; ++slot) {
            if (group[slot].upper == expected)
                return &group[slot];
        }
    }
    return 0;
}

int powerpc_hpt_map(struct PowerPCHashPageTable const* table, PowerPCHPTWord vsid, PowerPCHPTWord effective_address, PowerPCHPTWord physical_address, PowerPCHPTWord lower_flags)
{
    if (!valid_table(table, vsid, effective_address) || (physical_address & (POWERPC_HPT_PAGE_SIZE - 1)) != 0
        || (lower_flags & ~0xfffu) != 0 || powerpc_hpt_find(table, vsid, effective_address))
        return 0;

    PowerPCHPTWord hash = primary_hash(vsid, effective_address);
    for (PowerPCHPTWord secondary = 0; secondary < 2; ++secondary) {
        struct PowerPCHPTEntry* group = group_for(table, secondary ? ~hash : hash);
        for (PowerPCHPTWord slot = 0; slot < POWERPC_HPT_ENTRIES_PER_GROUP; ++slot) {
            if ((group[slot].upper & POWERPC_HPT_VALID) != 0)
                continue;
            group[slot].lower = physical_address | lower_flags;
            __asm__ volatile("eieio" : : : "memory");
            group[slot].upper = upper_word(vsid, effective_address, secondary);
            __asm__ volatile("sync" : : : "memory");
            return 1;
        }
    }
    return 0;
}

static void invalidate_entry(PowerPCHPTWord effective_address, struct PowerPCHPTEntry* entry)
{
    entry->upper = 0;
    __asm__ volatile("sync" : : : "memory");
    __asm__ volatile("tlbie %0; eieio; tlbsync; sync" : : "r"(effective_address) : "memory");
}

int powerpc_hpt_unmap(struct PowerPCHashPageTable const* table, PowerPCHPTWord vsid, PowerPCHPTWord effective_address)
{
    struct PowerPCHPTEntry* entry = powerpc_hpt_find(table, vsid, effective_address);
    if (!entry)
        return 0;
    invalidate_entry(effective_address, entry);
    return 1;
}

int powerpc_hpt_remap(struct PowerPCHashPageTable const* table, PowerPCHPTWord vsid, PowerPCHPTWord effective_address, PowerPCHPTWord physical_address, PowerPCHPTWord lower_flags)
{
    if ((physical_address & (POWERPC_HPT_PAGE_SIZE - 1)) != 0 || (lower_flags & ~0xfffu) != 0)
        return 0;
    struct PowerPCHPTEntry* entry = powerpc_hpt_find(table, vsid, effective_address);
    if (!entry)
        return 0;

    PowerPCHPTWord upper = entry->upper;
    invalidate_entry(effective_address, entry);
    entry->lower = physical_address | lower_flags;
    __asm__ volatile("eieio" : : : "memory");
    entry->upper = upper;
    __asm__ volatile("sync" : : : "memory");
    return 1;
}
