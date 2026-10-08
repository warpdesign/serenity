/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/MemoryLayout.h>

typedef unsigned int Word;
typedef int (*MarkerFunction)(void);

extern void powerpc_install_boot_bats(void);
extern void powerpc_enable_boot_translation(void);
extern void powerpc_disable_boot_translation(void);
extern int powerpc_high_alias_marker(void);

static volatile Word s_alias_probe_word = 0x1234abcd;

int powerpc_mmu_high_alias_roundtrip(void)
{
    Word machine_state;
    __asm__ volatile("mfmsr %0" : "=r"(machine_state));
    if ((machine_state & 0x30u) != 0)
        return 0;

    Word function_physical = (Word)(__UINTPTR_TYPE__)powerpc_high_alias_marker;
    Word data_physical = (Word)(__UINTPTR_TYPE__)&s_alias_probe_word;
    if (function_physical >= 0x01000000u || data_physical >= 0x01000000u)
        return 0;

    powerpc_install_boot_bats();
    powerpc_enable_boot_translation();

    MarkerFunction high_marker = (MarkerFunction)(__UINTPTR_TYPE__)(KERNEL_MAPPING_BASE + function_physical);
    volatile Word* high_data = (volatile Word*)(__UINTPTR_TYPE__)(KERNEL_MAPPING_BASE + data_physical);
    Word exceptions_before;
    __asm__ volatile("mfspr %0,273" : "=r"(exceptions_before));
    int executed_high = high_marker();
    Word exceptions_after;
    Word exception_status;
    __asm__ volatile("mfspr %0,273; mfspr %1,274" : "=r"(exceptions_after), "=r"(exception_status));
    Word original = s_alias_probe_word;
    Word high_read = *high_data;
    volatile unsigned char* high_data_bytes = (volatile unsigned char*)high_data;
    int high_is_big_endian = high_data_bytes[0] == 0x12 && high_data_bytes[1] == 0x34
        && high_data_bytes[2] == 0xab && high_data_bytes[3] == 0xcd;
    *high_data = original ^ 0x55aa55aa;
    __asm__ volatile("sync" : : : "memory");
    Word low_read = s_alias_probe_word;
    *high_data = original;

    Word uart_status;
    Word uart_status_address = 0xfe0002fdu;
    __asm__ volatile("lbz %0,0(%1)" : "=r"(uart_status) : "r"(uart_status_address) : "memory");

    Word translated_state;
    __asm__ volatile("mfmsr %0" : "=r"(translated_state));
    powerpc_disable_boot_translation();

    return executed_high && high_read == original && high_is_big_endian
        && low_read == (original ^ 0x55aa55aa) && exceptions_after == exceptions_before + 1
        && (exception_status & 0x00020030u) == 0x00020030u
        && (translated_state & 0x30u) == 0x30u && (uart_status & 0x20u) != 0;
}
