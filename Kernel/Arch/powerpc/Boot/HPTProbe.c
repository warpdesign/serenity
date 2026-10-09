/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/HashPageTable.h>
#include <Kernel/Arch/powerpc/MemoryLayout.h>

typedef unsigned int Word;
typedef int (*MarkerFunction)(void);

enum {
    PageSize = POWERPC_HPT_PAGE_SIZE,
    PageTableSize = 64 * 1024,
    PageTableGroupCount = PageTableSize / 64,
    PageTableEntriesPerGroup = POWERPC_HPT_ENTRIES_PER_GROUP,
    KernelVSID = 12,
    HighDataPage = KERNEL_MAPPING_BASE + 0x00800000,
    ReadOnlyPage = KERNEL_MAPPING_BASE + 0x00900000,
    UserTestAddress = 0x10000000,
    FirstUserVSID = 0x101,
    SecondUserVSID = 0x201,
    CollisionBase = KERNEL_MAPPING_BASE + 0x00401000,
    CollisionStride = 0x00400000,
    CollisionCount = 9,
    CollisionGroupCapacity = 2 * PageTableEntriesPerGroup,
    PageFlags = POWERPC_HPT_REFERENCED | POWERPC_HPT_CHANGED | POWERPC_HPT_COHERENT | POWERPC_HPT_READ_WRITE,
};

static struct PowerPCHPTEntry s_page_table[PageTableGroupCount * PageTableEntriesPerGroup]
    __attribute__((aligned(PageTableSize)));
static struct PowerPCHashPageTable s_hpt = { s_page_table, PageTableGroupCount };
static volatile Word s_hpt_payload_word __attribute__((aligned(PageSize))) = 0x89abcdef;
static volatile Word s_hpt_replacement_word __attribute__((aligned(PageSize))) = 0x13579bdf;
static int s_hpt_unmapped_fault_observed;

extern void powerpc_select_boot_hpt(Word table_physical);
extern void powerpc_enable_boot_translation(void);
extern void powerpc_disable_boot_translation(void);
extern int powerpc_high_alias_marker(void);
extern Word powerpc_probe_absent_high_alias(Word effective_address);
extern void powerpc_probe_read_only_store(Word effective_address, Word value);

static int map_high_page(Word effective_address, Word physical_address)
{
    return powerpc_hpt_map(&s_hpt, KernelVSID, effective_address, physical_address, PageFlags);
}

static struct PowerPCHPTEntry* find_high_page(Word effective_address)
{
    return powerpc_hpt_find(&s_hpt, KernelVSID, effective_address);
}

static int unmap_high_page(Word effective_address)
{
    return powerpc_hpt_unmap(&s_hpt, KernelVSID, effective_address);
}

static int remap_high_page(Word effective_address, Word physical_address)
{
    return powerpc_hpt_remap(&s_hpt, KernelVSID, effective_address, physical_address, PageFlags);
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

int powerpc_hpt_duplicate_mapping_rejected(void)
{
    Word replacement_physical = (Word)(__UINTPTR_TYPE__)&s_hpt_replacement_word;
    if (map_high_page(HighDataPage, replacement_physical))
        return 0;

    powerpc_enable_boot_translation();
    volatile Word* high_data = (volatile Word*)(__UINTPTR_TYPE__)HighDataPage;
    Word observed = *high_data;
    powerpc_disable_boot_translation();
    return observed == 0x89abcdefu;
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
    struct PowerPCHPTEntry* collision_entry = find_high_page(CollisionBase + (CollisionCount - 1) * CollisionStride);
    if (!collision_entry || (collision_entry->upper & POWERPC_HPT_SECONDARY_HASH) == 0)
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

int powerpc_hpt_group_reuse(void)
{
    Word replacement_physical = (Word)(__UINTPTR_TYPE__)&s_hpt_replacement_word;
    for (Word index = CollisionCount; index < CollisionGroupCapacity; ++index) {
        if (!map_high_page(CollisionBase + index * CollisionStride, (Word)(__UINTPTR_TYPE__)&s_hpt_payload_word))
            return 0;
    }

    Word replacement_alias = CollisionBase + CollisionGroupCapacity * CollisionStride;
    if (map_high_page(replacement_alias, replacement_physical))
        return 0;

    powerpc_enable_boot_translation();
    int passed = unmap_high_page(CollisionBase + (CollisionCount - 1) * CollisionStride)
        && map_high_page(replacement_alias, replacement_physical);
    if (passed) {
        volatile Word* alias = (volatile Word*)(__UINTPTR_TYPE__)replacement_alias;
        volatile Word* original_alias = (volatile Word*)(__UINTPTR_TYPE__)CollisionBase;
        passed = *alias == 0x13579bdfu && *original_alias == 0x89abcdefu;
    }
    Word fault_count_before;
    __asm__ volatile("mfspr %0,272" : "=r"(fault_count_before));
    Word absent_alias = CollisionBase + (CollisionCount - 1) * CollisionStride;
    Word fault_result = powerpc_probe_absent_high_alias(absent_alias);
    Word fault_count_after;
    Word fault_address;
    Word fault_status;
    __asm__ volatile("mfspr %0,272; mfspr %1,274; mfspr %2,273"
        : "=r"(fault_count_after), "=r"(fault_address), "=r"(fault_status));
    s_hpt_unmapped_fault_observed = fault_result == 0 && fault_count_after == fault_count_before + 1
        && fault_address == absent_alias && (fault_status & 0x40000000u) != 0;
    powerpc_disable_boot_translation();
    return passed;
}

int powerpc_hpt_unmapped_fault_observed(void)
{
    return s_hpt_unmapped_fault_observed;
}

int powerpc_hpt_read_only_fault_roundtrip(void)
{
    Word replacement_physical = (Word)(__UINTPTR_TYPE__)&s_hpt_replacement_word;
    Word read_only_flags = POWERPC_HPT_REFERENCED | POWERPC_HPT_CHANGED | POWERPC_HPT_COHERENT | POWERPC_HPT_READ_ONLY;
    if (!powerpc_hpt_map(&s_hpt, KernelVSID, ReadOnlyPage, replacement_physical, read_only_flags))
        return 0;

    powerpc_enable_boot_translation();
    volatile Word* alias = (volatile Word*)(__UINTPTR_TYPE__)ReadOnlyPage;
    Word value_before = *alias;
    Word fault_count_before;
    __asm__ volatile("mfspr %0,272" : "=r"(fault_count_before));
    powerpc_probe_read_only_store(ReadOnlyPage, 0xdeadbeefu);
    Word fault_count_after;
    Word fault_address;
    Word fault_status;
    __asm__ volatile("mfspr %0,272; mfspr %1,274; mfspr %2,273"
        : "=r"(fault_count_after), "=r"(fault_address), "=r"(fault_status));
    Word value_after = *alias;
    Word physical_value = s_hpt_replacement_word;
    powerpc_disable_boot_translation();

    return value_before == 0x13579bdfu && value_after == value_before && physical_value == value_before
        && fault_count_after == fault_count_before + 1 && fault_address == ReadOnlyPage
        && (fault_status & 0x0a000000u) == 0x0a000000u && (fault_status & 0x40000000u) == 0;
}

static void select_user_segment(Word vsid)
{
    __asm__ volatile("mtsr 1,%0; isync" : : "r"(vsid) : "memory");
}

int powerpc_hpt_vsid_switch_roundtrip(void)
{
    Word machine_state;
    Word original_segment;
    __asm__ volatile("mfmsr %0; mfsr %1,1" : "=r"(machine_state), "=r"(original_segment));
    if ((machine_state & 0x30u) != 0)
        return 0;

    Word first_physical = (Word)(__UINTPTR_TYPE__)&s_hpt_payload_word;
    Word second_physical = (Word)(__UINTPTR_TYPE__)&s_hpt_replacement_word;
    if (!powerpc_hpt_map(&s_hpt, FirstUserVSID, UserTestAddress, first_physical, PageFlags)
        || !powerpc_hpt_map(&s_hpt, SecondUserVSID, UserTestAddress, second_physical, PageFlags))
        return 0;

    volatile Word* alias = (volatile Word*)(__UINTPTR_TYPE__)UserTestAddress;
    powerpc_enable_boot_translation();
    select_user_segment(FirstUserVSID);
    Word first_read = *alias;
    *alias = 0x11223344u;
    __asm__ volatile("sync" : : : "memory");
    Word first_physical_read = s_hpt_payload_word;

    select_user_segment(SecondUserVSID);
    Word second_read = *alias;
    Word kernel_read = *(volatile Word*)(__UINTPTR_TYPE__)HighDataPage;
    *alias = 0x55667788u;
    __asm__ volatile("sync" : : : "memory");
    Word second_physical_read = s_hpt_replacement_word;

    select_user_segment(FirstUserVSID);
    Word first_again = *alias;
    *alias = 0x89abcdefu;
    select_user_segment(SecondUserVSID);
    Word second_again = *alias;
    *alias = 0x13579bdfu;
    powerpc_disable_boot_translation();
    select_user_segment(original_segment);

    int unmapped = powerpc_hpt_unmap(&s_hpt, FirstUserVSID, UserTestAddress)
        && powerpc_hpt_unmap(&s_hpt, SecondUserVSID, UserTestAddress);
    return first_read == 0x89abcdefu && second_read == 0x13579bdfu && kernel_read == 0x11223344u
        && first_physical_read == 0x11223344u && second_physical_read == 0x55667788u
        && first_again == 0x11223344u && second_again == 0x55667788u
        && s_hpt_payload_word == 0x89abcdefu && s_hpt_replacement_word == 0x13579bdfu
        && unmapped;
}
