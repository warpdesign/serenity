/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibELF/Arch/breakpoint.h>

extern "C" int powerpc_elf_breakpoint_test()
{
    unsigned before;
    unsigned after;
    __asm__ volatile("mfspr %0,273" : "=r"(before));
    ELF::breakpoint_trap();
    __asm__ volatile("mfspr %0,273" : "=r"(after));
    return after == before + 1;
}
