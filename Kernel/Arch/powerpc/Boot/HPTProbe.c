/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/ExceptionState.h>
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
    UserProbeCodePage = 0x10010000,
    UserProbeDataPage = 0x10020000,
    UserProbeStackPage = 0x10030000,
    UserProbeVSID = 0x301,
    CollisionBase = KERNEL_MAPPING_BASE + 0x00401000,
    CollisionStride = 0x00400000,
    CollisionCount = 9,
    CollisionGroupCapacity = 2 * PageTableEntriesPerGroup,
    KernelPageFlags = POWERPC_HPT_REFERENCED | POWERPC_HPT_CHANGED | POWERPC_HPT_COHERENT | POWERPC_HPT_SUPERVISOR_ONLY,
    UserPageFlags = POWERPC_HPT_REFERENCED | POWERPC_HPT_CHANGED | POWERPC_HPT_COHERENT | POWERPC_HPT_READ_WRITE,
};

static struct PowerPCHPTEntry s_page_table[PageTableGroupCount * PageTableEntriesPerGroup]
    __attribute__((aligned(PageTableSize)));
static struct PowerPCHashPageTable s_hpt = { s_page_table, PageTableGroupCount };
static volatile Word s_hpt_payload_word __attribute__((aligned(PageSize))) = 0x89abcdef;
static volatile Word s_hpt_replacement_word __attribute__((aligned(PageSize))) = 0x13579bdf;
static volatile Word s_user_data_page[PageSize / sizeof(Word)] __attribute__((aligned(PageSize)));
static volatile Word s_user_stack[PageSize / sizeof(Word)] __attribute__((aligned(PageSize)));
static int s_hpt_unmapped_fault_observed;
static int s_user_stack_observed;
static int s_user_syscall_resume_observed;
static int s_user_exception_frame_observed;

volatile struct PowerPCExceptionState powerpc_user_probe_frame;

volatile Word powerpc_user_probe_saved_msr;
volatile Word powerpc_user_probe_resume_address;
volatile Word powerpc_user_probe_observed_srr0;
volatile Word powerpc_user_probe_observed_srr1;
volatile Word powerpc_user_probe_saved_supervisor_stack;
volatile Word powerpc_user_probe_observed_user_stack;
volatile Word powerpc_user_probe_saved_supervisor_cr;
volatile Word powerpc_user_probe_syscall_count;

extern void powerpc_select_boot_hpt(Word table_physical);
extern void powerpc_enable_boot_translation(void);
extern void powerpc_disable_boot_translation(void);
extern int powerpc_high_alias_marker(void);
extern Word powerpc_probe_absent_high_alias(Word effective_address);
extern void powerpc_probe_read_only_store(Word effective_address, Word value);
extern Word powerpc_user_probe_start[];
extern Word powerpc_user_probe_end[];
extern Word powerpc_user_probe_after_first_syscall[];
extern Word powerpc_user_probe_after_second_syscall[];
extern Word powerpc_enter_user_probe(Word user_entry, Word protected_address, Word user_data_address, Word value, Word user_stack_top);

static int map_high_page(Word effective_address, Word physical_address)
{
    return powerpc_hpt_map(&s_hpt, KernelVSID, effective_address, physical_address, KernelPageFlags);
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
    return powerpc_hpt_remap(&s_hpt, KernelVSID, effective_address, physical_address, KernelPageFlags);
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

static void replace_test_mapping_without_tlb_flush(struct PowerPCHPTEntry* entry, Word upper, Word lower)
{
    entry->upper = 0;
    __asm__ volatile("sync" : : : "memory");
    entry->lower = lower;
    __asm__ volatile("eieio" : : : "memory");
    entry->upper = upper;
    __asm__ volatile("sync" : : : "memory");
}

int powerpc_hpt_processor_tlb_test(void (*flush_page)(Word), void (*flush_all)(void))
{
    struct PowerPCHPTEntry* entry = find_high_page(HighDataPage);
    Word replacement_physical = (Word)(__UINTPTR_TYPE__)&s_hpt_replacement_word;
    if (!entry || !flush_page || !flush_all || (replacement_physical & (PageSize - 1)) != 0)
        return 0;

    Word original_upper = entry->upper;
    Word original_lower = entry->lower;
    powerpc_enable_boot_translation();
    volatile Word* alias = (volatile Word*)(__UINTPTR_TYPE__)HighDataPage;
    int passed = *alias == s_hpt_payload_word;

    replace_test_mapping_without_tlb_flush(entry, original_upper, replacement_physical | (original_lower & (PageSize - 1)));
    flush_page(HighDataPage);
    passed = passed && *alias == s_hpt_replacement_word;

    replace_test_mapping_without_tlb_flush(entry, original_upper, original_lower);
    flush_all();
    passed = passed && *alias == s_hpt_payload_word;
    powerpc_disable_boot_translation();
    return passed;
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
    if (!powerpc_hpt_map(&s_hpt, FirstUserVSID, UserTestAddress, first_physical, UserPageFlags)
        || !powerpc_hpt_map(&s_hpt, SecondUserVSID, UserTestAddress, second_physical, UserPageFlags))
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

int powerpc_hpt_kernel_protection_roundtrip(void)
{
    Word original_segment;
    __asm__ volatile("mfsr %0,12" : "=r"(original_segment));
    if ((original_segment & 0x00ffffffu) != KernelVSID)
        return 0;

    powerpc_enable_boot_translation();
    Word protected_segment = original_segment | 0x60000000u; // Ks and Kp select the protected key.
    __asm__ volatile("mtsr 12,%0; isync" : : "r"(protected_segment) : "memory");

    Word fault_count_before;
    __asm__ volatile("mfspr %0,272" : "=r"(fault_count_before));
    Word fault_result = powerpc_probe_absent_high_alias(HighDataPage);
    Word fault_count_after;
    Word fault_address;
    Word fault_status;
    __asm__ volatile("mfspr %0,272; mfspr %1,274; mfspr %2,273"
        : "=r"(fault_count_after), "=r"(fault_address), "=r"(fault_status));

    __asm__ volatile("mtsr 12,%0; isync" : : "r"(original_segment) : "memory");
    Word readable_again = *(volatile Word*)(__UINTPTR_TYPE__)HighDataPage;
    powerpc_disable_boot_translation();

    return (original_segment & 0x20000000u) != 0 && fault_result == 0
        && fault_count_after == fault_count_before + 1 && fault_address == HighDataPage
        && (fault_status & 0x48000000u) == 0x08000000u && readable_again == 0x89abcdefu;
}

int powerpc_hpt_user_mode_roundtrip(void)
{
    Word machine_state;
    Word original_segment;
    __asm__ volatile("mfmsr %0; mfsr %1,1" : "=r"(machine_state), "=r"(original_segment));
    if ((machine_state & 0x4030u) != 0)
        return 0;

    Word code_physical = (Word)(__UINTPTR_TYPE__)powerpc_user_probe_start;
    Word code_length = (Word)(__UINTPTR_TYPE__)powerpc_user_probe_end - code_physical;
    Word first_resume_offset = (Word)(__UINTPTR_TYPE__)powerpc_user_probe_after_first_syscall - code_physical;
    Word second_resume_offset = (Word)(__UINTPTR_TYPE__)powerpc_user_probe_after_second_syscall - code_physical;
    if ((code_physical & (PageSize - 1)) != 0 || code_length == 0 || code_length > PageSize
        || first_resume_offset >= code_length || second_resume_offset >= code_length
        || code_physical >= 0x01000000u - PageSize)
        return 0;
    if (!powerpc_hpt_map(&s_hpt, UserProbeVSID, UserProbeCodePage, code_physical, UserPageFlags))
        return 0;
    Word data_physical = (Word)(__UINTPTR_TYPE__)s_user_data_page;
    if (!powerpc_hpt_map(&s_hpt, UserProbeVSID, UserProbeDataPage, data_physical, UserPageFlags)) {
        (void)powerpc_hpt_unmap(&s_hpt, UserProbeVSID, UserProbeCodePage);
        return 0;
    }
    Word stack_physical = (Word)(__UINTPTR_TYPE__)s_user_stack;
    if (stack_physical >= 0x01000000u || !powerpc_hpt_map(&s_hpt, UserProbeVSID, UserProbeStackPage, stack_physical, UserPageFlags)) {
        (void)powerpc_hpt_unmap(&s_hpt, UserProbeVSID, UserProbeCodePage);
        (void)powerpc_hpt_unmap(&s_hpt, UserProbeVSID, UserProbeDataPage);
        return 0;
    }

    powerpc_user_probe_observed_srr0 = 0;
    powerpc_user_probe_observed_srr1 = 0;
    powerpc_user_probe_observed_user_stack = 0;
    powerpc_user_probe_syscall_count = 0;
    select_user_segment(0x20000000u | UserProbeVSID); // Kp=1 for problem-state accesses.
    Word fault_count_before;
    Word supervisor_stack_before;
    __asm__ volatile("mfspr %0,272" : "=r"(fault_count_before));
    __asm__ volatile("mr %0,1" : "=r"(supervisor_stack_before));
    Word user_result = powerpc_enter_user_probe(UserProbeCodePage, HighDataPage, UserProbeDataPage, 0x2468ace0u, UserProbeStackPage + PageSize - 16);
    Word fault_count_after;
    Word fault_address;
    Word fault_status;
    Word returned_state;
    Word supervisor_stack_after;
    __asm__ volatile("mfspr %0,272; mfspr %1,274; mfspr %2,273; mfmsr %3"
        : "=r"(fault_count_after), "=r"(fault_address), "=r"(fault_status), "=r"(returned_state));
    __asm__ volatile("mr %0,1" : "=r"(supervisor_stack_after));
    Word user_data_value = s_user_data_page[0];
    Word user_saved_r8 = s_user_data_page[1];
    Word user_saved_r9 = s_user_data_page[2];
    Word user_saved_r10 = s_user_data_page[3];
    Word user_saved_r11 = s_user_data_page[4];
    Word user_saved_r12 = s_user_data_page[5];
    Word user_saved_cr = s_user_data_page[6];
    Word user_saved_ctr = s_user_data_page[7];
    Word user_saved_xer = s_user_data_page[8];
    Word user_saved_lr = s_user_data_page[9];
    Word user_stack_after_syscall = s_user_data_page[10];
    for (Word index = 0; index < 11; ++index)
        s_user_data_page[index] = 0;
    Word user_stack_back_chain = s_user_stack[(PageSize - 32) / sizeof(Word)];
    Word user_stack_value = s_user_stack[(PageSize - 24) / sizeof(Word)];
    s_user_stack[(PageSize - 32) / sizeof(Word)] = 0;
    s_user_stack[(PageSize - 24) / sizeof(Word)] = 0;
    select_user_segment(original_segment);
    int unmapped = powerpc_hpt_unmap(&s_hpt, UserProbeVSID, UserProbeCodePage)
        && powerpc_hpt_unmap(&s_hpt, UserProbeVSID, UserProbeDataPage)
        && powerpc_hpt_unmap(&s_hpt, UserProbeVSID, UserProbeStackPage);

    s_user_stack_observed = user_stack_back_chain == UserProbeStackPage + PageSize - 16
        && user_stack_value == 0x2468ace0u
        && powerpc_user_probe_observed_user_stack == UserProbeStackPage + PageSize - 16
        && powerpc_user_probe_saved_supervisor_stack == supervisor_stack_before
        && supervisor_stack_after == supervisor_stack_before;
    s_user_syscall_resume_observed = powerpc_user_probe_syscall_count == 2
        && (user_saved_cr & 0xe0000000u) == 0x80000000u
        && user_saved_r8 == 8 && user_saved_r9 == 9 && user_saved_r10 == 10
        && user_saved_r11 == 5 && user_saved_r12 == 7
        && user_saved_ctr == 10 && user_saved_xer == 0 && user_saved_lr == 0x11223344u
        && user_stack_after_syscall == UserProbeStackPage + PageSize - 16;
    s_user_exception_frame_observed = powerpc_user_probe_frame.gpr[1] == UserProbeStackPage + PageSize - 16
        && powerpc_user_probe_frame.gpr[3] == 0x2468ace0u
        && powerpc_user_probe_frame.gpr[5] == UserProbeDataPage
        && powerpc_user_probe_frame.gpr[8] == 0x11223344u
        && powerpc_user_probe_frame.gpr[9] == 9 && powerpc_user_probe_frame.gpr[10] == 10
        && powerpc_user_probe_frame.gpr[11] == 5 && powerpc_user_probe_frame.gpr[12] == 7
        && (powerpc_user_probe_frame.cr & 0xe0000000u) == 0x80000000u
        && powerpc_user_probe_frame.ctr == 10 && powerpc_user_probe_frame.xer == 0
        && powerpc_user_probe_frame.lr == 0x11223344u
        && powerpc_user_probe_frame.srr0 == UserProbeCodePage + second_resume_offset
        && (powerpc_user_probe_frame.srr1 & 0x4030u) == 0x4030u;

    return user_result == 0x2468ace0u && user_data_value == 0x2468ace0u
        && fault_count_after == fault_count_before + 1
        && fault_address == HighDataPage && (fault_status & 0x48000000u) == 0x08000000u
        && (powerpc_user_probe_observed_srr1 & 0x4030u) == 0x4030u
        && powerpc_user_probe_observed_srr0 == UserProbeCodePage + first_resume_offset
        && (returned_state & 0x4030u) == 0 && s_hpt_payload_word == 0x89abcdefu
        && unmapped;
}

int powerpc_user_stack_observed(void)
{
    return s_user_stack_observed;
}

int powerpc_user_syscall_resume_observed(void)
{
    return s_user_syscall_resume_observed;
}

int powerpc_user_exception_frame_observed(void)
{
    return s_user_exception_frame_observed;
}
