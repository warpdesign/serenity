/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/TLB.h>

void powerpc_flush_tlb_page(unsigned int effective_address)
{
    asm volatile("sync" : : : "memory");
    asm volatile("tlbie %0; eieio; tlbsync; sync" : : "r"(effective_address) : "memory");
}

void powerpc_flush_entire_tlb(void)
{
    // The 750 and 74xx do not implement tlbia. EA bits 14-19 index all
    // 64 instruction and data TLB sets.
    asm volatile("sync" : : : "memory");
    for (unsigned int index = 0; index < 64; ++index) {
        unsigned int address = index << 14;
        asm volatile("tlbie %0" : : "r"(address) : "memory");
    }
    asm volatile("eieio; tlbsync; sync" : : : "memory");
}
