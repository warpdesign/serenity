/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/HashPageTable.h>
#include <Kernel/Arch/powerpc/TLB.h>

static_assert(sizeof(PowerPCHPTWord) == 4, "PowerPC HPT word size");
static_assert(sizeof(PowerPCHPTEntry) == 8, "PowerPC HPT entry size");

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
        | (secondary ? static_cast<PowerPCHPTWord>(POWERPC_HPT_SECONDARY_HASH) : 0u);
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

int powerpc_hpt_map_with_eviction(struct PowerPCHashPageTable const* table, PowerPCHPTWord vsid, PowerPCHPTWord effective_address, PowerPCHPTWord physical_address, PowerPCHPTWord lower_flags, PowerPCHPTEvictionPredicate may_evict, void* context)
{
    if (!valid_table(table, vsid, effective_address) || (physical_address & (POWERPC_HPT_PAGE_SIZE - 1)) != 0
        || (lower_flags & ~0xfffu) != 0 || powerpc_hpt_find(table, vsid, effective_address))
        return 0;

    if (powerpc_hpt_map(table, vsid, effective_address, physical_address, lower_flags))
        return 1;
    if (!may_evict)
        return 0;

    // The HPT is a cache of the software mappings. When both hash groups are
    // full, replace a mapping that the fault handlers can reconstruct.
    static PowerPCHPTWord next_victim;
    PowerPCHPTWord hash = primary_hash(vsid, effective_address);
    for (PowerPCHPTWord attempt = 0; attempt < 2 * POWERPC_HPT_ENTRIES_PER_GROUP; ++attempt) {
        PowerPCHPTWord candidate = (next_victim + attempt) % (2 * POWERPC_HPT_ENTRIES_PER_GROUP);
        PowerPCHPTWord secondary = candidate / POWERPC_HPT_ENTRIES_PER_GROUP;
        auto* group = group_for(table, secondary ? ~hash : hash);
        auto* entry = &group[candidate % POWERPC_HPT_ENTRIES_PER_GROUP];
        auto upper = entry->upper;
        if (!(upper & POWERPC_HPT_VALID))
            continue;

        auto victim_vsid = (upper >> 7) & 0x00ffffffu;
        auto group_index = (secondary ? ~hash : hash) & (table->group_count - 1);
        auto victim_primary_hash = (upper & POWERPC_HPT_SECONDARY_HASH) ? ~group_index : group_index;
        auto victim_page_index = ((upper & 0x3fu) << 10) | ((victim_primary_hash ^ victim_vsid) & 0x3ffu);
        auto victim_address = ((victim_vsid & 0xfu) << 28) | (victim_page_index << 12);
        if (!may_evict(victim_vsid, victim_address, context))
            continue;

        next_victim = (candidate + 1) % (2 * POWERPC_HPT_ENTRIES_PER_GROUP);
        entry->upper = 0;
        // A victim may belong to another address space, so invalidate every
        // TLB set rather than relying on the current segment registers.
        powerpc_flush_entire_tlb();
        entry->lower = physical_address | lower_flags;
        __asm__ volatile("eieio" : : : "memory");
        entry->upper = upper_word(vsid, effective_address, secondary);
        __asm__ volatile("sync" : : : "memory");
        return 1;
    }
    return 0;
}

static void invalidate_entry(PowerPCHPTWord effective_address, struct PowerPCHPTEntry* entry)
{
    entry->upper = 0;
    powerpc_flush_tlb_page(effective_address);
}

int powerpc_hpt_unmap(struct PowerPCHashPageTable const* table, PowerPCHPTWord vsid, PowerPCHPTWord effective_address)
{
    struct PowerPCHPTEntry* entry = powerpc_hpt_find(table, vsid, effective_address);
    if (!entry)
        return 0;
    invalidate_entry(effective_address, entry);
    return 1;
}

int powerpc_hpt_purge_vsid_block(struct PowerPCHashPageTable const* table, PowerPCHPTWord first_vsid, PowerPCHPTWord vsid_count)
{
    if (!table || !table->entries || table->group_count == 0
        || (table->group_count & (table->group_count - 1)) != 0
        || table->group_count > 0xffffffffu / POWERPC_HPT_ENTRIES_PER_GROUP
        || vsid_count == 0 || vsid_count > 0x01000000u
        || first_vsid > 0x01000000u - vsid_count)
        return -1;

    int removed = 0;
    PowerPCHPTWord entry_count = table->group_count * POWERPC_HPT_ENTRIES_PER_GROUP;
    for (PowerPCHPTWord index = 0; index < entry_count; ++index) {
        struct PowerPCHPTEntry* entry = &table->entries[index];
        PowerPCHPTWord upper = entry->upper;
        if ((upper & POWERPC_HPT_VALID) == 0)
            continue;
        PowerPCHPTWord vsid = (upper >> 7) & 0x00ffffffu;
        if (vsid >= first_vsid && vsid - first_vsid < vsid_count) {
            entry->upper = 0;
            ++removed;
        }
    }

    if (removed != 0)
        powerpc_flush_entire_tlb();
    return removed;
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
