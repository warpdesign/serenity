/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/MemoryLayout.h>

typedef unsigned int Word;
typedef int (*MarkerFunction)(void);

enum {
    PageSize = 4096,
    PageTableSize = 64 * 1024,
    PageTableGroupCount = PageTableSize / 64,
    PageTableEntriesPerGroup = 8,
    KernelVSID = 12,
    HighDataPage = KERNEL_MAPPING_BASE + 0x00800000,
    CollisionBase = KERNEL_MAPPING_BASE + 0x00401000,
    CollisionStride = 0x00400000,
    CollisionCount = 9,
    Valid = 0x80000000u,
    SecondaryHash = 0x40,
    Referenced = 0x100,
    Changed = 0x80,
    Coherent = 0x10,
    ReadWrite = 0x2,
};

struct PageTableEntry {
    volatile Word upper;
    volatile Word lower;
};

_Static_assert(sizeof(struct PageTableEntry) == 8, "PowerPC HPT entry size");

static struct PageTableEntry s_page_table[PageTableGroupCount * PageTableEntriesPerGroup]
    __attribute__((aligned(PageTableSize)));
static volatile Word s_hpt_payload_word __attribute__((aligned(PageSize))) = 0x89abcdef;
static volatile Word s_hpt_replacement_word __attribute__((aligned(PageSize))) = 0x13579bdf;

extern void powerpc_select_boot_hpt(Word table_physical);
extern void powerpc_enable_boot_translation(void);
extern void powerpc_disable_boot_translation(void);
extern int powerpc_high_alias_marker(void);

static int map_high_page(Word effective_address, Word physical_address)
{
    Word primary_hash = (KernelVSID & 0x7ffffu) ^ ((effective_address >> 12) & 0xffffu);
    for (Word secondary = 0; secondary < 2; ++secondary) {
        Word hash = secondary ? ~primary_hash : primary_hash;
        Word group_index = hash & (PageTableGroupCount - 1);
        struct PageTableEntry* group = &s_page_table[group_index * PageTableEntriesPerGroup];
        for (Word slot = 0; slot < PageTableEntriesPerGroup; ++slot) {
            if ((group[slot].upper & Valid) != 0)
                continue;
            group[slot].lower = (physical_address & ~(Word)(PageSize - 1)) | Referenced | Changed | Coherent | ReadWrite;
            __asm__ volatile("eieio" : : : "memory");
            group[slot].upper = Valid | (KernelVSID << 7) | ((effective_address >> 22) & 0x3fu)
                | (secondary ? SecondaryHash : 0);
            __asm__ volatile("sync" : : : "memory");
            return 1;
        }
    }
    return 0;
}

static struct PageTableEntry* find_high_page(Word effective_address)
{
    Word primary_hash = (KernelVSID & 0x7ffffu) ^ ((effective_address >> 12) & 0xffffu);
    for (Word secondary = 0; secondary < 2; ++secondary) {
        Word hash = secondary ? ~primary_hash : primary_hash;
        Word group_index = hash & (PageTableGroupCount - 1);
        struct PageTableEntry* group = &s_page_table[group_index * PageTableEntriesPerGroup];
        Word upper = Valid | (KernelVSID << 7) | ((effective_address >> 22) & 0x3fu)
            | (secondary ? SecondaryHash : 0);
        for (Word slot = 0; slot < PageTableEntriesPerGroup; ++slot) {
            if (group[slot].upper == upper)
                return &group[slot];
        }
    }
    return 0;
}

static int remap_high_page(Word effective_address, Word physical_address)
{
    struct PageTableEntry* entry = find_high_page(effective_address);
    if (!entry)
        return 0;

    Word upper = entry->upper;
    entry->upper = upper & ~Valid;
    __asm__ volatile("sync" : : : "memory");
    __asm__ volatile("tlbie %0; eieio; tlbsync; sync" : : "r"(effective_address) : "memory");
    entry->lower = (physical_address & ~(Word)(PageSize - 1)) | Referenced | Changed | Coherent | ReadWrite;
    __asm__ volatile("eieio" : : : "memory");
    entry->upper = upper;
    __asm__ volatile("sync" : : : "memory");
    return 1;
}

int powerpc_hpt_high_alias_roundtrip(void)
{
    Word machine_state;
    __asm__ volatile("mfmsr %0" : "=r"(machine_state));
    if ((machine_state & 0x30u) != 0)
        return 0;

    Word table_physical = (Word)(__UINTPTR_TYPE__)s_page_table;
    Word function_physical = (Word)(__UINTPTR_TYPE__)powerpc_high_alias_marker;
    Word payload_physical = (Word)(__UINTPTR_TYPE__)&s_hpt_payload_word;
    if ((table_physical & (PageTableSize - 1)) != 0 || table_physical > 0x01000000u - PageTableSize
        || function_physical >= 0x01000000u - PageSize || payload_physical >= 0x01000000u)
        return 0;

    Word function_page = function_physical & ~(Word)(PageSize - 1);
    if (!map_high_page(KERNEL_MAPPING_BASE + function_page, function_page)
        || !map_high_page(KERNEL_MAPPING_BASE + function_page + PageSize, function_page + PageSize)
        || !map_high_page(HighDataPage, payload_physical))
        return 0;

    powerpc_select_boot_hpt(table_physical);
    powerpc_enable_boot_translation();

    MarkerFunction high_marker = (MarkerFunction)(__UINTPTR_TYPE__)(KERNEL_MAPPING_BASE + function_physical);
    Word exceptions_before;
    __asm__ volatile("mfspr %0,273" : "=r"(exceptions_before));
    int executed_high = high_marker();
    Word exceptions_after;
    Word exception_status;
    __asm__ volatile("mfspr %0,273; mfspr %1,274" : "=r"(exceptions_after), "=r"(exception_status));

    volatile Word* high_data = (volatile Word*)(__UINTPTR_TYPE__)(HighDataPage + (payload_physical & (PageSize - 1)));
    Word high_read = *high_data;
    *high_data = 0x76543210u;
    __asm__ volatile("sync" : : : "memory");
    Word low_read = s_hpt_payload_word;
    *high_data = 0x89abcdefu;

    Word translated_state;
    __asm__ volatile("mfmsr %0" : "=r"(translated_state));
    powerpc_disable_boot_translation();

    Word installed_sdr1;
    Word instruction_bat_high;
    Word data_bat_high;
    Word high_segment;
    __asm__ volatile("mfspr %0,25; mfspr %1,530; mfspr %2,538; mfsr %3,12"
        : "=r"(installed_sdr1), "=r"(instruction_bat_high), "=r"(data_bat_high), "=r"(high_segment));

    return executed_high && high_read == 0x89abcdefu && low_read == 0x76543210u
        && exceptions_after == exceptions_before + 1 && (exception_status & 0x00020030u) == 0x00020030u
        && (translated_state & 0x30u) == 0x30u && installed_sdr1 == table_physical
        && instruction_bat_high == 0 && data_bat_high == 0 && (high_segment & 0x00ffffffu) == KernelVSID;
}

int powerpc_hpt_mapping_lifecycle(void)
{
    Word payload_physical = (Word)(__UINTPTR_TYPE__)&s_hpt_payload_word;
    Word replacement_physical = (Word)(__UINTPTR_TYPE__)&s_hpt_replacement_word;
    if ((payload_physical & (PageSize - 1)) != 0 || (replacement_physical & (PageSize - 1)) != 0
        || payload_physical >= 0x01000000u || replacement_physical >= 0x01000000u
        || payload_physical == replacement_physical)
        return 0;

    for (Word index = 0; index < CollisionCount; ++index) {
        if (!map_high_page(CollisionBase + index * CollisionStride, payload_physical))
            return 0;
    }
    struct PageTableEntry* collision_entry = find_high_page(CollisionBase + (CollisionCount - 1) * CollisionStride);
    if (!collision_entry || (collision_entry->upper & SecondaryHash) == 0)
        return 0;

    powerpc_enable_boot_translation();
    int passed = 1;
    for (Word index = 0; index < CollisionCount; ++index) {
        volatile Word* alias = (volatile Word*)(__UINTPTR_TYPE__)(CollisionBase + index * CollisionStride);
        if (*alias != 0x89abcdefu)
            passed = 0;
    }

    volatile Word* high_data = (volatile Word*)(__UINTPTR_TYPE__)HighDataPage;
    if (*high_data != 0x89abcdefu || !remap_high_page(HighDataPage, replacement_physical)) {
        passed = 0;
    } else {
        if (*high_data != 0x13579bdfu)
            passed = 0;
        *high_data = 0x2468ace0u;
        __asm__ volatile("sync" : : : "memory");
        if (s_hpt_replacement_word != 0x2468ace0u || s_hpt_payload_word != 0x89abcdefu)
            passed = 0;
        if (!remap_high_page(HighDataPage, payload_physical) || *high_data != 0x89abcdefu)
            passed = 0;
        s_hpt_replacement_word = 0x13579bdfu;
    }
    powerpc_disable_boot_translation();
    return passed;
}
