/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/ExceptionState.h>
#include <Kernel/Arch/powerpc/Boot/ProbeAddresses.h>
#include <Kernel/Arch/powerpc/HashPageTable.h>
#include <Kernel/Arch/powerpc/MemoryLayout.h>
#include <Kernel/Arch/powerpc/Pegasos2/HardwareAddresses.h>
#include <Kernel/Arch/powerpc/PageDirectoryBackend.h>
#include <Kernel/Arch/powerpc/PhysicalPageArray.h>
#include <Kernel/Arch/powerpc/SegmentRegisters.h>
#include <Kernel/Arch/powerpc/VSIDAllocator.h>
#include <Kernel/Sections.h>

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
    QuickmapTestAddress = KERNEL_MAPPING_BASE + KERNEL_PT1024_OFFSET + 3 * KERNEL_MAX_CPU_COUNT * PageSize,
    PhysicalArrayTestAddress = 0xc2400000u,
    HighUserFramePage = 0xc2600000u,
    IRQStackTestAddress = POWERPC_HPT_IRQ_STACK_ADDRESS,
    ExceptionStackTestAddress = POWERPC_HPT_EXCEPTION_STACK_ADDRESS,
    IntackTestAddress = POWERPC_HPT_INTACK_ADDRESS,
    PICIOTestAddress = POWERPC_HPT_PIC_IO_ADDRESS,
    UserTestAddress = 0x10000000,
    UserProbeCodePage = 0x10010000,
    UserProbeDataPage = 0x10020000,
    UserProbeStackPage = 0x10030000,
    UserProbeVSID = 0x301,
    CollisionBase = KERNEL_MAPPING_BASE + 0x00401000,
    CollisionStride = 0x00400000,
    CollisionCount = 9,
    CollisionGroupCapacity = 2 * PageTableEntriesPerGroup,
    KernelPageFlags = POWERPC_HPT_REFERENCED | POWERPC_HPT_CHANGED | POWERPC_HPT_COHERENT | POWERPC_HPT_SUPERVISOR_ONLY,
    IOPageFlags = POWERPC_HPT_REFERENCED | POWERPC_HPT_CHANGED | POWERPC_HPT_CACHE_INHIBITED | POWERPC_HPT_GUARDED | POWERPC_HPT_SUPERVISOR_ONLY,
    UserPageFlags = POWERPC_HPT_REFERENCED | POWERPC_HPT_CHANGED | POWERPC_HPT_COHERENT | POWERPC_HPT_READ_WRITE,
};

static struct PowerPCHPTEntry s_page_table[PageTableGroupCount * PageTableEntriesPerGroup]
    __attribute__((aligned(PageTableSize)));
static struct PowerPCHashPageTable s_hpt = { s_page_table, PageTableGroupCount };

Word powerpc_quickmap_probe_address(void)
{
    return QuickmapTestAddress;
}

Word powerpc_hpt_table_physical(void)
{
    return (Word)(__UINTPTR_TYPE__)s_page_table;
}

Word powerpc_hpt_group_count(void)
{
    return PageTableGroupCount;
}
static volatile Word s_hpt_payload_word __attribute__((aligned(PageSize))) = 0x89abcdef;
static volatile Word s_hpt_replacement_word __attribute__((aligned(PageSize))) = 0x13579bdf;
static volatile Word s_quickmap_payload[PageSize / sizeof(Word)] __attribute__((aligned(PageSize))) = { 0x2468ace0u };
static volatile Word s_physical_array_payload[3 * PageSize / sizeof(Word)] __attribute__((aligned(PageSize))) = {
    [0] = 0x10203040u,
    [PageSize / sizeof(Word)] = 0x50607080u,
    [2 * PageSize / sizeof(Word)] = 0x90abcdefu,
};
static volatile Word s_user_data_page[PageSize / sizeof(Word)] __attribute__((aligned(PageSize)));
static volatile Word s_user_stack[PageSize / sizeof(Word)] __attribute__((aligned(PageSize)));
static struct PowerPCExceptionState s_user_return_frame __attribute__((aligned(PageSize)));
static int s_hpt_unmapped_fault_observed;
static int s_user_stack_observed;
static int s_user_syscall_resume_observed;
static int s_user_exception_frame_observed;
static int s_user_external_irq_observed;
volatile Word powerpc_hpt_user_high_debug;
volatile Word powerpc_hpt_user_return_debug;
volatile Word powerpc_probe_supervisor_return_srr0;
volatile Word powerpc_probe_supervisor_return_srr1;
volatile Word powerpc_probe_saved_sc_cr;
volatile Word powerpc_probe_saved_sc_r11;
volatile Word powerpc_probe_dsi_high_segment;

volatile struct PowerPCExceptionState powerpc_user_probe_frame;

volatile Word powerpc_user_probe_saved_msr;
volatile Word powerpc_user_probe_resume_address;
volatile Word powerpc_user_probe_observed_srr0;
volatile Word powerpc_user_probe_observed_srr1;
volatile Word powerpc_user_probe_saved_supervisor_stack;
volatile Word powerpc_user_probe_observed_user_stack;
volatile Word powerpc_user_probe_entry_high_segment;
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
extern Word powerpc_user_irq_probe_start[];
extern Word powerpc_user_irq_probe_end[];
extern Word powerpc_user_probe_after_first_syscall[];
extern Word powerpc_user_probe_after_second_syscall[];
extern Word powerpc_enter_user_probe(Word user_entry, Word protected_address, Word user_data_address, Word value, Word user_stack_top, Word extra_msr_bits);
extern Word powerpc_enter_user_exception_frame_probe(struct PowerPCExceptionState* frame);
extern Word powerpc_enter_user_translated_exception_frame_probe(struct PowerPCExceptionState* frame, Word return_kind);
extern char powerpc_user_translated_return_resume[];
extern int powerpc_test_map_kernel_page(struct PowerPCHashPageTable const*, Word effective_address, Word physical_address);
extern unsigned char powerpc_probe_high_stack[];
extern unsigned char powerpc_probe_exception_stack[];
extern char __text_end[];
extern volatile Word powerpc_probe_use_high_irq_handler;
extern int powerpc_run_hpt_stack_external_irq_probe(Word timebase_frequency);
extern void powerpc_irq_probe_reset(void);
extern int powerpc_irq_prepare_user_probe(void);
extern void powerpc_irq_finish_user_probe(void);
extern Word powerpc_irq_probe_count(void);
extern Word powerpc_irq_probe_number(void);
extern Word powerpc_irq_probe_saved_srr0(void);
extern Word powerpc_irq_probe_saved_srr1(void);
extern Word powerpc_irq_probe_saved_stack(void);
extern Word powerpc_irq_probe_dispatch_caller(void);
extern volatile Word powerpc_probe_user_irq_signal_address;

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

int powerpc_hpt_external_irq_stack_roundtrip(Word timebase_frequency)
{
    Word physical_stack = (Word)(__UINTPTR_TYPE__)powerpc_probe_high_stack;
    Word sdr1;
    Word instruction_bat_high;
    Word data_bat_high;
    __asm__ volatile("mfspr %0,25; mfspr %1,530; mfspr %2,538"
        : "=r"(sdr1), "=r"(instruction_bat_high), "=r"(data_bat_high));
    if (!timebase_frequency || (physical_stack & (PageSize - 1)) != 0
        || physical_stack >= 0x01000000u - PageSize || sdr1 != powerpc_hpt_table_physical()
        || instruction_bat_high != 0 || data_bat_high != 0)
        return 0;

    if (!map_high_page(IRQStackTestAddress, physical_stack))
        return 0;
    int passed = powerpc_run_hpt_stack_external_irq_probe(timebase_frequency);
    int unmapped = unmap_high_page(IRQStackTestAddress);
    __asm__ volatile("mfspr %0,530; mfspr %1,538" : "=r"(instruction_bat_high), "=r"(data_bat_high));
    return passed && unmapped && instruction_bat_high == 0 && data_bat_high == 0;
}

static int probe_user_irq_from_high_handler(void)
{
    Word code_physical = (Word)(__UINTPTR_TYPE__)powerpc_user_irq_probe_start;
    Word code_length = (Word)(__UINTPTR_TYPE__)powerpc_user_irq_probe_end - code_physical;
    Word data_physical = (Word)(__UINTPTR_TYPE__)s_user_data_page;
    Word stack_physical = (Word)(__UINTPTR_TYPE__)s_user_stack;
    if ((code_physical & (PageSize - 1)) || !code_length || code_length > PageSize
        || code_physical >= 0x01000000u - PageSize || data_physical >= 0x01000000u
        || stack_physical >= 0x01000000u)
        return 0;

    if (!powerpc_hpt_map(&s_hpt, UserProbeVSID, UserProbeCodePage, code_physical, UserPageFlags))
        return 0;
    if (!powerpc_hpt_map(&s_hpt, UserProbeVSID, UserProbeDataPage, data_physical, UserPageFlags)) {
        (void)powerpc_hpt_unmap(&s_hpt, UserProbeVSID, UserProbeCodePage);
        return 0;
    }
    if (!powerpc_hpt_map(&s_hpt, UserProbeVSID, UserProbeStackPage, stack_physical, UserPageFlags)) {
        (void)powerpc_hpt_unmap(&s_hpt, UserProbeVSID, UserProbeCodePage);
        (void)powerpc_hpt_unmap(&s_hpt, UserProbeVSID, UserProbeDataPage);
        return 0;
    }

    Word original_segment = powerpc_read_segment_register(1);
    s_user_data_page[0] = 0;
    s_user_data_page[1] = 0;
    s_user_data_page[2] = 0;
    powerpc_user_probe_syscall_count = 0;
    powerpc_irq_probe_reset();
    powerpc_probe_user_irq_signal_address = data_physical;
    int prepared = powerpc_irq_prepare_user_probe();
    Word user_result = 0;
    Word supervisor_stack_before = 0;
    Word supervisor_stack_after = 0;
    Word returned_state = 0;
    if (prepared) {
        powerpc_write_segment_register(1, 0x20000000u | UserProbeVSID);
        __asm__ volatile("mr %0,1" : "=r"(supervisor_stack_before));
        user_result = powerpc_enter_user_probe(UserProbeCodePage, HighDataPage, UserProbeDataPage, 0,
            UserProbeStackPage + PageSize - 16, 0x8000u);
        __asm__ volatile("mr %0,1; mfmsr %1" : "=r"(supervisor_stack_after), "=r"(returned_state));
        powerpc_write_segment_register(1, original_segment);
    }
    powerpc_probe_user_irq_signal_address = 0;
    powerpc_irq_finish_user_probe();

    Word signal = s_user_data_page[0];
    Word observed_signal = s_user_data_page[1];
    Word user_stack = s_user_data_page[2];
    s_user_data_page[0] = 0;
    s_user_data_page[1] = 0;
    s_user_data_page[2] = 0;
    int unmapped = powerpc_hpt_unmap(&s_hpt, UserProbeVSID, UserProbeCodePage)
        && powerpc_hpt_unmap(&s_hpt, UserProbeVSID, UserProbeDataPage)
        && powerpc_hpt_unmap(&s_hpt, UserProbeVSID, UserProbeStackPage);
    Word saved_srr1 = powerpc_irq_probe_saved_srr1();
    Word saved_srr0 = powerpc_irq_probe_saved_srr0();
    Word high_caller = powerpc_irq_probe_dispatch_caller();
    return prepared && unmapped && user_result == 0 && powerpc_user_probe_syscall_count == 2
        && signal == 1 && observed_signal == 1 && user_stack == UserProbeStackPage + PageSize - 16
        && powerpc_irq_probe_count() == 1 && powerpc_irq_probe_number() == 0
        && (saved_srr1 & 0xc030u) == 0xc030u
        && saved_srr0 >= UserProbeCodePage && saved_srr0 < UserProbeCodePage + code_length
        && powerpc_irq_probe_saved_stack() == UserProbeStackPage + PageSize - 16
        && high_caller >= 0xc0000000u && high_caller < 0xc1000000u
        && supervisor_stack_before == supervisor_stack_after && (returned_state & 0xc030u) == 0;
}

int powerpc_hpt_user_external_irq_observed(void)
{
    return s_user_external_irq_observed;
}

int powerpc_hpt_external_irq_high_handler_roundtrip(Word timebase_frequency)
{
    enum { TextStart = 0x00200000u, MaximumTextPages = 128 };
    Word text_end = (Word)(__UINTPTR_TYPE__)__text_end;
    Word high_stack = (Word)(__UINTPTR_TYPE__)powerpc_probe_high_stack;
    Word exception_stack = (Word)(__UINTPTR_TYPE__)powerpc_probe_exception_stack;
    Word instruction_bat_high;
    Word data_bat_high;
    __asm__ volatile("mfspr %0,530; mfspr %1,538" : "=r"(instruction_bat_high), "=r"(data_bat_high));
    if (!timebase_frequency || text_end <= TextStart || text_end > TextStart + MaximumTextPages * PageSize
        || (high_stack & (PageSize - 1)) || (exception_stack & (PageSize - 1))
        || high_stack >= 0x01000000u - PageSize || exception_stack >= 0x01000000u - PageSize
        || instruction_bat_high != 0 || data_bat_high != 0)
        return 0;

    Word text_pages = (text_end - TextStart + PageSize - 1) / PageSize;
    unsigned char new_text_mapping[MaximumTextPages] = { 0 };
    int high_stack_mapped = 0;
    int exception_stack_mapped = 0;
    int intack_mapped = 0;
    int pic_io_mapped = 0;
    int passed = 0;
    int cleanup_ok = 1;

    for (Word index = 0; index < text_pages; ++index) {
        Word physical = TextStart + index * PageSize;
        Word high = KERNEL_MAPPING_BASE + physical;
        struct PowerPCHPTEntry* existing = find_high_page(high);
        if (existing) {
            if ((existing->lower & ~(Word)(PageSize - 1)) != physical)
                goto cleanup;
            continue;
        }
        if (!map_high_page(high, physical))
            goto cleanup;
        new_text_mapping[index] = 1;
    }

    if (!map_high_page(IRQStackTestAddress, high_stack))
        goto cleanup;
    high_stack_mapped = 1;
    if (!map_high_page(ExceptionStackTestAddress, exception_stack))
        goto cleanup;
    exception_stack_mapped = 1;
    if (!powerpc_hpt_map(&s_hpt, KernelVSID, IntackTestAddress, PEGASOS2_INTACK_PHYSICAL_BASE, IOPageFlags))
        goto cleanup;
    intack_mapped = 1;
    if (!powerpc_hpt_map(&s_hpt, KernelVSID, PICIOTestAddress, PEGASOS2_PCI_IO_PHYSICAL_BASE, IOPageFlags))
        goto cleanup;
    pic_io_mapped = 1;
    struct PowerPCHPTEntry* intack_entry = find_high_page(IntackTestAddress);
    struct PowerPCHPTEntry* pic_io_entry = find_high_page(PICIOTestAddress);
    Word required_io_flags = POWERPC_HPT_CACHE_INHIBITED | POWERPC_HPT_GUARDED;
    if (!intack_entry || !pic_io_entry
        || (intack_entry->lower & required_io_flags) != required_io_flags
        || (pic_io_entry->lower & required_io_flags) != required_io_flags)
        goto cleanup;

    powerpc_probe_use_high_irq_handler = 1;
    __asm__ volatile("sync" : : : "memory");
    passed = powerpc_run_hpt_stack_external_irq_probe(timebase_frequency);
    if (passed)
        s_user_external_irq_observed = probe_user_irq_from_high_handler();
    powerpc_probe_use_high_irq_handler = 0;
    __asm__ volatile("sync" : : : "memory");

cleanup:
    if (pic_io_mapped && !unmap_high_page(PICIOTestAddress))
        cleanup_ok = 0;
    if (intack_mapped && !unmap_high_page(IntackTestAddress))
        cleanup_ok = 0;
    if (exception_stack_mapped && !unmap_high_page(ExceptionStackTestAddress))
        cleanup_ok = 0;
    if (high_stack_mapped && !unmap_high_page(IRQStackTestAddress))
        cleanup_ok = 0;
    for (Word index = 0; index < text_pages; ++index) {
        if (new_text_mapping[index] && !unmap_high_page(KERNEL_MAPPING_BASE + TextStart + index * PageSize))
            cleanup_ok = 0;
    }
    __asm__ volatile("mfspr %0,530; mfspr %1,538" : "=r"(instruction_bat_high), "=r"(data_bat_high));
    return passed && cleanup_ok && instruction_bat_high == 0 && data_bat_high == 0;
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

int powerpc_hpt_quickmap_roundtrip(void)
{
    Word machine_state;
    __asm__ volatile("mfmsr %0" : "=r"(machine_state));
    Word physical = (Word)(__UINTPTR_TYPE__)s_quickmap_payload;
    if ((machine_state & 0x30u) != 0 || (physical & (PageSize - 1)) != 0 || physical >= 0x01000000u)
        return 0;

    Word original_segments[16];
    for (Word segment = 0; segment < 16; ++segment)
        original_segments[segment] = powerpc_read_segment_register(segment);
    if (!powerpc_test_map_kernel_page(&s_hpt, QuickmapTestAddress, physical))
        return 0;

    powerpc_activate_kernel_segments();
    powerpc_enable_boot_translation();
    volatile Word* alias = (volatile Word*)(__UINTPTR_TYPE__)QuickmapTestAddress;
    Word initial = *alias;
    *alias = 0x12345678u;
    __asm__ volatile("sync" : : : "memory");
    Word physical_read = s_quickmap_payload[0];

    int unmapped = powerpc_hpt_unmap(&s_hpt, 15, QuickmapTestAddress);
    Word faults_before;
    __asm__ volatile("mfspr %0,272" : "=r"(faults_before));
    Word fault_result = powerpc_probe_absent_high_alias(QuickmapTestAddress);
    Word faults_after;
    Word fault_address;
    Word fault_status;
    __asm__ volatile("mfspr %0,272; mfspr %1,274; mfspr %2,273"
        : "=r"(faults_after), "=r"(fault_address), "=r"(fault_status));
    powerpc_disable_boot_translation();
    for (Word segment = 0; segment < 16; ++segment)
        powerpc_write_segment_register(segment, original_segments[segment]);
    s_quickmap_payload[0] = 0x2468ace0u;

    return initial == 0x2468ace0u && physical_read == 0x12345678u && unmapped
        && fault_result == 0 && faults_after == faults_before + 1
        && fault_address == QuickmapTestAddress && (fault_status & 0x40000000u) != 0;
}

int powerpc_hpt_physical_array_roundtrip(void)
{
    Word machine_state;
    __asm__ volatile("mfmsr %0" : "=r"(machine_state));
    Word physical = (Word)(__UINTPTR_TYPE__)s_physical_array_payload;
    if ((machine_state & 0x30u) != 0 || (physical & (PageSize - 1)) != 0 || physical >= 0x01000000u - 3 * PageSize)
        return 0;
    if (!powerpc_map_kernel_page_array(&s_hpt, PhysicalArrayTestAddress, physical, 3))
        return 0;

    powerpc_enable_boot_translation();
    volatile Word* alias = (volatile Word*)(__UINTPTR_TYPE__)PhysicalArrayTestAddress;
    Word first = alias[0];
    Word second = alias[PageSize / sizeof(Word)];
    Word third = alias[2 * PageSize / sizeof(Word)];
    alias[PageSize / sizeof(Word)] = 0x11223344u;
    __asm__ volatile("sync" : : : "memory");
    Word physical_read = s_physical_array_payload[PageSize / sizeof(Word)];
    powerpc_disable_boot_translation();

    int unmapped = powerpc_unmap_kernel_page_array(&s_hpt, PhysicalArrayTestAddress, 3);
    s_physical_array_payload[PageSize / sizeof(Word)] = 0x50607080u;
    return first == 0x10203040u && second == 0x50607080u && third == 0x90abcdefu
        && physical_read == 0x11223344u && unmapped;
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

static int user_segments_match(Word vsid_base)
{
    for (Word segment = 0; segment < POWERPC_USER_SEGMENT_COUNT; ++segment) {
        if (powerpc_read_segment_register(segment) != vsid_base + segment)
            return 0;
    }
    return 1;
}

int powerpc_hpt_vsid_switch_roundtrip(void)
{
    Word machine_state;
    __asm__ volatile("mfmsr %0" : "=r"(machine_state));
    if ((machine_state & 0x30u) != 0)
        return 0;

    Word original_segments[POWERPC_USER_SEGMENT_COUNT];
    for (Word segment = 0; segment < POWERPC_USER_SEGMENT_COUNT; ++segment)
        original_segments[segment] = powerpc_read_segment_register(segment);
    Word original_kernel_segments[4];
    for (Word segment = 12; segment < 16; ++segment)
        original_kernel_segments[segment - 12] = powerpc_read_segment_register(segment);
    Word original_kernel_segment = original_kernel_segments[0];

    struct PowerPCVSIDAllocator allocator;
    powerpc_vsid_allocator_initialize(&allocator);
    struct PowerPCPageDirectoryBackend first_directory;
    struct PowerPCPageDirectoryBackend second_directory;
    if (!powerpc_page_directory_backend_initialize(&first_directory, &s_hpt, &allocator)
        || !powerpc_page_directory_backend_initialize(&second_directory, &s_hpt, &allocator))
        return 0;
    Word first_base = first_directory.user_vsid_base;
    Word second_base = second_directory.user_vsid_base;
    Word first_vsid = powerpc_page_directory_backend_vsid_for(&first_directory, UserTestAddress);
    Word second_vsid = powerpc_page_directory_backend_vsid_for(&second_directory, UserTestAddress);

    if (powerpc_user_vsid_base_valid(0)
        || powerpc_user_vsid_base_valid(first_vsid)
        || powerpc_user_vsid_base_valid(0x01000000u)
        || !powerpc_user_vsid_base_valid(first_base)
        || !powerpc_user_vsid_base_valid(second_base)
        || second_base - first_base != POWERPC_VSID_BLOCK_SIZE
        || first_vsid != first_base + 1 || second_vsid != second_base + 1
        || powerpc_page_directory_backend_vsid_for(&first_directory, HighDataPage) != 0
        || !powerpc_user_vsid_base_valid(0x00fffff0u)
        || powerpc_activate_user_segments(first_vsid)
        || powerpc_read_segment_register(1) != original_segments[1]
        || powerpc_activate_user_high_segments(first_vsid)
        || powerpc_read_segment_register(12) != original_kernel_segment)
        return 0;

    Word first_physical = (Word)(__UINTPTR_TYPE__)&s_hpt_payload_word;
    Word second_physical = (Word)(__UINTPTR_TYPE__)&s_hpt_replacement_word;
    Word third_physical = (Word)(__UINTPTR_TYPE__)s_user_data_page;
    if ((third_physical & (PageSize - 1)) != 0 || third_physical >= 0x01000000u
        || powerpc_page_directory_backend_map(&first_directory, HighDataPage, first_physical, UserPageFlags)
        || !powerpc_page_directory_backend_map(&first_directory, UserTestAddress, first_physical, UserPageFlags)
        || !powerpc_page_directory_backend_map(&second_directory, UserTestAddress, second_physical, UserPageFlags))
        return 0;

    volatile Word* alias = (volatile Word*)(__UINTPTR_TYPE__)UserTestAddress;
    powerpc_enable_boot_translation();
    int first_activated = powerpc_page_directory_backend_activate(&first_directory);
    int first_segments_match = user_segments_match(first_base)
        && powerpc_read_segment_register(12) == original_kernel_segment;
    Word first_read = *alias;
    *alias = 0x11223344u;
    __asm__ volatile("sync" : : : "memory");
    Word first_physical_read = s_hpt_payload_word;

    int second_activated = powerpc_page_directory_backend_activate(&second_directory);
    int second_segments_match = user_segments_match(second_base)
        && powerpc_read_segment_register(12) == original_kernel_segment;
    Word second_read = *alias;
    Word kernel_read = *(volatile Word*)(__UINTPTR_TYPE__)HighDataPage;
    *alias = 0x55667788u;
    __asm__ volatile("sync" : : : "memory");
    Word second_physical_read = s_hpt_replacement_word;

    powerpc_page_directory_backend_activate(&first_directory);
    Word first_again = *alias;
    int remapped = powerpc_page_directory_backend_remap(&first_directory, UserTestAddress, third_physical, UserPageFlags);
    Word third_read = *alias;
    *alias = 0xaabbccddu;
    __asm__ volatile("sync" : : : "memory");
    Word third_physical_read = s_user_data_page[0];
    int restored = powerpc_page_directory_backend_remap(&first_directory, UserTestAddress, first_physical, UserPageFlags);
    Word first_after_remap = *alias;
    *alias = 0x89abcdefu;
    powerpc_page_directory_backend_activate(&second_directory);
    Word second_again = *alias;
    *alias = 0x13579bdfu;
    Word purged = powerpc_page_directory_backend_purge(&first_directory);
    int first_mapping_gone = powerpc_hpt_find(&s_hpt, first_vsid, UserTestAddress) == 0;
    Word second_after_purge = *alias;
    powerpc_disable_boot_translation();
    s_user_data_page[0] = 0;
    powerpc_activate_kernel_segments();
    int kernel_segments_match = 1;
    for (Word segment = 0; segment < 16; ++segment) {
        if (powerpc_read_segment_register(segment) != (0x20000000u | segment))
            kernel_segments_match = 0;
    }
    for (Word segment = 0; segment < POWERPC_USER_SEGMENT_COUNT; ++segment)
        powerpc_write_segment_register(segment, original_segments[segment]);
    for (Word segment = 12; segment < 16; ++segment)
        powerpc_write_segment_register(segment, original_kernel_segments[segment - 12]);

    int unmapped = powerpc_page_directory_backend_unmap(&second_directory, UserTestAddress);
    return first_activated && second_activated && first_segments_match && second_segments_match && kernel_segments_match
        && powerpc_read_segment_register(12) == original_kernel_segment
        && first_read == 0x89abcdefu && second_read == 0x13579bdfu && kernel_read == 0x11223344u
        && first_physical_read == 0x11223344u && second_physical_read == 0x55667788u
        && first_again == 0x11223344u && second_again == 0x55667788u
        && purged == 1 && first_mapping_gone && second_after_purge == 0x13579bdfu
        && remapped && restored && third_read == 0 && third_physical_read == 0xaabbccddu
        && first_after_remap == 0x11223344u
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
    __asm__ volatile("mfmsr %0" : "=r"(machine_state));
    original_segment = powerpc_read_segment_register(1);
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
    powerpc_write_segment_register(1, 0x20000000u | UserProbeVSID); // Kp=1 for problem-state accesses.
    Word fault_count_before;
    Word supervisor_stack_before;
    __asm__ volatile("mfspr %0,272" : "=r"(fault_count_before));
    __asm__ volatile("mr %0,1" : "=r"(supervisor_stack_before));
    Word user_result = powerpc_enter_user_probe(UserProbeCodePage, HighDataPage, UserProbeDataPage, 0x2468ace0u, UserProbeStackPage + PageSize - 16, 0);
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
    powerpc_write_segment_register(1, original_segment);
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

int powerpc_hpt_user_high_vsid_isolation_roundtrip(void)
{
    Word machine_state;
    __asm__ volatile("mfmsr %0" : "=r"(machine_state));
    if ((machine_state & 0x4030u) != 0)
        return 0;

    Word user_base = UserProbeVSID & ~(Word)(POWERPC_VSID_BLOCK_SIZE - 1);
    if (!powerpc_user_vsid_base_valid(user_base))
        return 0;
    struct PowerPCHPTEntry* kernel_entry = find_high_page(HighDataPage);
    if (!kernel_entry || (kernel_entry->lower & 3u) != POWERPC_HPT_SUPERVISOR_ONLY)
        return 0;
    Word original_lower = kernel_entry->lower;
    Word physical = original_lower & ~(Word)(PageSize - 1);
    Word read_only_flags = (original_lower & (PageSize - 1) & ~3u) | POWERPC_HPT_READ_ONLY;
    if (!powerpc_hpt_remap(&s_hpt, KernelVSID, HighDataPage, physical, read_only_flags))
        return 0;

    Word code_physical = (Word)(__UINTPTR_TYPE__)powerpc_user_probe_start;
    Word data_physical = (Word)(__UINTPTR_TYPE__)s_user_data_page;
    Word stack_physical = (Word)(__UINTPTR_TYPE__)s_user_stack;
    int mapped_code = powerpc_hpt_map(&s_hpt, UserProbeVSID, UserProbeCodePage, code_physical, UserPageFlags);
    int mapped_data = mapped_code && powerpc_hpt_map(&s_hpt, UserProbeVSID, UserProbeDataPage, data_physical, UserPageFlags);
    int mapped_stack = mapped_data && powerpc_hpt_map(&s_hpt, UserProbeVSID, UserProbeStackPage, stack_physical, UserPageFlags);
    if (!mapped_stack) {
        if (mapped_data)
            (void)powerpc_hpt_unmap(&s_hpt, UserProbeVSID, UserProbeDataPage);
        if (mapped_code)
            (void)powerpc_hpt_unmap(&s_hpt, UserProbeVSID, UserProbeCodePage);
        (void)powerpc_hpt_remap(&s_hpt, KernelVSID, HighDataPage, physical, original_lower & (PageSize - 1));
        return 0;
    }

    Word original_user_segment = powerpc_read_segment_register(1);
    Word original_high_segments[4];
    for (Word segment = 12; segment < 16; ++segment)
        original_high_segments[segment - 12] = powerpc_read_segment_register(segment);
    powerpc_write_segment_register(1, 0x20000000u | UserProbeVSID);

    powerpc_enable_boot_translation();
    Word supervisor_read = *(volatile Word*)(__UINTPTR_TYPE__)HighDataPage;
    int rejected_translated_switch = !powerpc_activate_user_high_segments(user_base)
        && powerpc_read_segment_register(12) == original_high_segments[0];
    powerpc_disable_boot_translation();

    // With the shared high VSID, PP=3 would expose a read-only kernel page.
    powerpc_user_probe_syscall_count = 0;
    Word exposed_faults_before;
    __asm__ volatile("mfspr %0,272" : "=r"(exposed_faults_before));
    Word exposed_read = powerpc_enter_user_probe(UserProbeCodePage, HighDataPage, UserProbeDataPage,
        0x2468ace0u, UserProbeStackPage + PageSize - 16, 0);
    Word exposed_faults_after;
    __asm__ volatile("mfspr %0,272" : "=r"(exposed_faults_after));

    int isolated_segments = powerpc_activate_user_high_segments(user_base);
    for (Word segment = 12; segment < 16; ++segment) {
        if (powerpc_read_segment_register(segment) != (0x20000000u | (user_base + segment)))
            isolated_segments = 0;
    }
    int user_low_segment_unchanged = powerpc_read_segment_register(1) == (0x20000000u | UserProbeVSID);
    int no_user_high_mapping = powerpc_hpt_find(&s_hpt, user_base + 12, HighDataPage) == 0;

    powerpc_user_probe_syscall_count = 0;
    Word isolated_faults_before;
    __asm__ volatile("mfspr %0,272" : "=r"(isolated_faults_before));
    Word isolated_read = powerpc_enter_user_probe(UserProbeCodePage, HighDataPage, UserProbeDataPage,
        0x2468ace0u, UserProbeStackPage + PageSize - 16, 0);
    Word isolated_faults_after;
    Word fault_address;
    Word fault_status;
    __asm__ volatile("mfspr %0,272; mfspr %1,274; mfspr %2,273"
        : "=r"(isolated_faults_after), "=r"(fault_address), "=r"(fault_status));
    Word second_syscall_high_segment = powerpc_user_probe_entry_high_segment;

    powerpc_activate_kernel_high_segments();
    int kernel_segments_restored = 1;
    for (Word segment = 12; segment < 16; ++segment) {
        if (powerpc_read_segment_register(segment) != (0x20000000u | segment))
            kernel_segments_restored = 0;
        powerpc_write_segment_register(segment, original_high_segments[segment - 12]);
    }
    powerpc_write_segment_register(1, original_user_segment);
    powerpc_enable_boot_translation();
    Word supervisor_read_again = *(volatile Word*)(__UINTPTR_TYPE__)HighDataPage;
    powerpc_disable_boot_translation();

    int restored = powerpc_hpt_remap(&s_hpt, KernelVSID, HighDataPage, physical, original_lower & (PageSize - 1));
    int unmapped = powerpc_hpt_unmap(&s_hpt, UserProbeVSID, UserProbeCodePage)
        && powerpc_hpt_unmap(&s_hpt, UserProbeVSID, UserProbeDataPage)
        && powerpc_hpt_unmap(&s_hpt, UserProbeVSID, UserProbeStackPage);
    powerpc_hpt_user_high_debug = (supervisor_read == 0x89abcdefu && supervisor_read_again == 0x89abcdefu ? 1u : 0u)
        | (exposed_read == 0x89abcdefu ? 1u << 1 : 0u)
        | (exposed_faults_after == exposed_faults_before ? 1u << 2 : 0u)
        | (isolated_segments ? 1u << 3 : 0u)
        | (user_low_segment_unchanged ? 1u << 4 : 0u)
        | (no_user_high_mapping ? 1u << 5 : 0u)
        | (isolated_read == 0x2468ace0u ? 1u << 6 : 0u)
        | (isolated_faults_after == isolated_faults_before + 1 ? 1u << 7 : 0u)
        | (fault_address == HighDataPage ? 1u << 8 : 0u)
        | ((fault_status & 0x40000000u) != 0 ? 1u << 9 : 0u)
        | (kernel_segments_restored ? 1u << 10 : 0u)
        | (restored ? 1u << 11 : 0u)
        | (unmapped ? 1u << 12 : 0u)
        | (rejected_translated_switch ? 1u << 13 : 0u);
    powerpc_hpt_user_high_debug |= second_syscall_high_segment == (0x20000000u | (user_base + 12)) ? 1u << 14 : 0u;
    powerpc_hpt_user_high_debug |= powerpc_probe_dsi_high_segment == (0x20000000u | 12) ? 1u << 15 : 0u;
    return powerpc_hpt_user_high_debug == 0xffffu;
}

static int user_exception_return_roundtrip(int return_kind)
{
    powerpc_hpt_user_return_debug = 0x10000u;
    Word machine_state;
    __asm__ volatile("mfmsr %0" : "=r"(machine_state));
    if ((machine_state & 0x4030u) != 0)
        return 0;
    powerpc_hpt_user_return_debug = 0x20000u;

    struct PowerPCHPTEntry* kernel_entry = find_high_page(HighDataPage);
    if (!kernel_entry || (kernel_entry->lower & 3u) != POWERPC_HPT_SUPERVISOR_ONLY)
        return 0;
    powerpc_hpt_user_return_debug = 0x30000u;
    Word original_lower = kernel_entry->lower;
    Word physical = original_lower & ~(Word)(PageSize - 1);
    Word read_only_flags = (original_lower & (PageSize - 1) & ~3u) | POWERPC_HPT_READ_ONLY;
    if (!powerpc_hpt_remap(&s_hpt, KernelVSID, HighDataPage, physical, read_only_flags))
        return 0;
    powerpc_hpt_user_return_debug = 0x40000u;

    int high_frame_mapped = return_kind != 2 || map_high_page(HighUserFramePage, (Word)(__UINTPTR_TYPE__)&s_user_return_frame);
    if (!high_frame_mapped) {
        (void)powerpc_hpt_remap(&s_hpt, KernelVSID, HighDataPage, physical, original_lower & (PageSize - 1));
        return 0;
    }

    Word code_physical = (Word)(__UINTPTR_TYPE__)powerpc_user_probe_start;
    Word data_physical = (Word)(__UINTPTR_TYPE__)s_user_data_page;
    Word stack_physical = (Word)(__UINTPTR_TYPE__)s_user_stack;
    int mapped_code = powerpc_hpt_map(&s_hpt, UserProbeVSID, UserProbeCodePage, code_physical, UserPageFlags);
    int mapped_data = mapped_code && powerpc_hpt_map(&s_hpt, UserProbeVSID, UserProbeDataPage, data_physical, UserPageFlags);
    int mapped_stack = mapped_data && powerpc_hpt_map(&s_hpt, UserProbeVSID, UserProbeStackPage, stack_physical, UserPageFlags);
    if (!mapped_stack) {
        if (mapped_data)
            (void)powerpc_hpt_unmap(&s_hpt, UserProbeVSID, UserProbeDataPage);
        if (mapped_code)
            (void)powerpc_hpt_unmap(&s_hpt, UserProbeVSID, UserProbeCodePage);
        if (return_kind == 2)
            (void)unmap_high_page(HighUserFramePage);
        (void)powerpc_hpt_remap(&s_hpt, KernelVSID, HighDataPage, physical, original_lower & (PageSize - 1));
        return 0;
    }
    powerpc_hpt_user_return_debug = 0x50000u;

    for (Word index = 0; index < 32; ++index)
        s_user_return_frame.gpr[index] = 0;
    s_user_return_frame.gpr[1] = UserProbeStackPage + PageSize - 16;
    s_user_return_frame.gpr[2] = 0x10293847u;
    s_user_return_frame.gpr[4] = HighDataPage;
    s_user_return_frame.gpr[5] = UserProbeDataPage;
    s_user_return_frame.gpr[6] = 0x2468ace0u;
    s_user_return_frame.gpr[14] = 0x31415926u;
    s_user_return_frame.cr = 0;
    s_user_return_frame.ctr = 0;
    s_user_return_frame.xer = 0;
    s_user_return_frame.lr = 0;
    s_user_return_frame.srr0 = UserProbeCodePage;
    s_user_return_frame.srr1 = 0x6030u;

    Word original_user_segment = powerpc_read_segment_register(1);
    Word original_high_segments[4];
    for (Word segment = 12; segment < 16; ++segment)
        original_high_segments[segment - 12] = powerpc_read_segment_register(segment);
    powerpc_write_segment_register(1, 0x20000000u | UserProbeVSID);
    powerpc_user_probe_syscall_count = 0;
    powerpc_user_probe_observed_srr1 = 0;
    powerpc_probe_supervisor_return_srr0 = 0;
    powerpc_probe_supervisor_return_srr1 = 0;
    s_user_data_page[0] = 0;

    Word fault_count_before;
    Word supervisor_stack_before;
    __asm__ volatile("mfspr %0,272; mr %1,1" : "=r"(fault_count_before), "=r"(supervisor_stack_before));
    Word result = return_kind != 0
        ? powerpc_enter_user_translated_exception_frame_probe(
              return_kind == 2 ? (struct PowerPCExceptionState*)(__UINTPTR_TYPE__)HighUserFramePage : &s_user_return_frame,
              return_kind == 2 ? 0x5353u : 0x5352u)
        : powerpc_enter_user_exception_frame_probe(&s_user_return_frame);
    Word fault_count_after;
    Word fault_address;
    Word returned_state;
    Word supervisor_stack_after;
    __asm__ volatile("mfspr %0,272; mfspr %1,274; mfmsr %2; mr %3,1"
        : "=r"(fault_count_after), "=r"(fault_address), "=r"(returned_state), "=r"(supervisor_stack_after));

    int kernel_high_restored = 1;
    for (Word segment = 12; segment < 16; ++segment) {
        if (powerpc_read_segment_register(segment) != (0x20000000u | segment))
            kernel_high_restored = 0;
        powerpc_write_segment_register(segment, original_high_segments[segment - 12]);
    }
    powerpc_write_segment_register(1, original_user_segment);

    Word checks = (result == 0x2468ace0u ? 1u : 0u)
        | (s_user_data_page[0] == 0x2468ace0u ? 1u << 1 : 0u)
        | (fault_count_after == fault_count_before + 1 ? 1u << 2 : 0u)
        | (fault_address == HighDataPage ? 1u << 3 : 0u)
        | (powerpc_user_probe_syscall_count == 2 ? 1u << 4 : 0u)
        | ((powerpc_user_probe_observed_srr1 & 0x6030u) == 0x6030u ? 1u << 5 : 0u)
        | (powerpc_user_probe_frame.gpr[1] == UserProbeStackPage + PageSize - 16 ? 1u << 6 : 0u)
        | (powerpc_user_probe_frame.gpr[2] == 0x10293847u ? 1u << 7 : 0u)
        | (powerpc_user_probe_frame.gpr[14] == 0x31415926u ? 1u << 8 : 0u)
        | ((powerpc_user_probe_frame.srr1 & 0x6030u) == 0x6030u ? 1u << 9 : 0u)
        | (powerpc_user_probe_saved_supervisor_stack == supervisor_stack_before - 128 ? 1u << 10 : 0u)
        | (supervisor_stack_after == supervisor_stack_before ? 1u << 11 : 0u)
        | ((returned_state & 0x4030u) == 0 ? 1u << 12 : 0u)
        | (kernel_high_restored ? 1u << 13 : 0u);

    for (Word index = 0; index < 11; ++index)
        s_user_data_page[index] = 0;
    s_user_stack[(PageSize - 32) / sizeof(Word)] = 0;
    s_user_stack[(PageSize - 24) / sizeof(Word)] = 0;
    int restored = powerpc_hpt_remap(&s_hpt, KernelVSID, HighDataPage, physical, original_lower & (PageSize - 1));
    int unmapped = powerpc_hpt_unmap(&s_hpt, UserProbeVSID, UserProbeCodePage)
        && powerpc_hpt_unmap(&s_hpt, UserProbeVSID, UserProbeDataPage)
        && powerpc_hpt_unmap(&s_hpt, UserProbeVSID, UserProbeStackPage);
    powerpc_hpt_user_return_debug = checks | (restored ? 1u << 14 : 0u) | (unmapped ? 1u << 15 : 0u);
    if (return_kind != 0) {
        powerpc_hpt_user_return_debug |= powerpc_probe_supervisor_return_srr0 == (Word)(__UINTPTR_TYPE__)powerpc_user_translated_return_resume ? 1u << 16 : 0u;
        powerpc_hpt_user_return_debug |= (powerpc_probe_supervisor_return_srr1 & 0x4030u) == 0x30u ? 1u << 17 : 0u;
    }
    if (return_kind == 2)
        powerpc_hpt_user_return_debug |= unmap_high_page(HighUserFramePage) ? 1u << 18 : 0u;
    Word expected_checks = 0xffffu;
    if (return_kind != 0)
        expected_checks |= 0x30000u;
    if (return_kind == 2)
        expected_checks |= 0x40000u;
    return powerpc_hpt_user_return_debug == expected_checks;
}

int powerpc_hpt_user_exception_return_roundtrip(void)
{
    return user_exception_return_roundtrip(0);
}

int powerpc_hpt_translated_user_exception_return_roundtrip(void)
{
    return user_exception_return_roundtrip(1);
}

int powerpc_hpt_high_frame_user_exception_return_roundtrip(void)
{
    return user_exception_return_roundtrip(2);
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
