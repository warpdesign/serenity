/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibSystem/syscall.h>

extern "C" {
volatile u32 powerpc_syscall_observed[5] {};
volatile u32 powerpc_syscall_response {};
extern u32 powerpc_syscall_invoke_vector_start[];
extern u32 powerpc_syscall_invoke_vector_end[];
}

static void flush_syscall_vector()
{
    constexpr FlatPtr vector_address = 0xc00;
    __asm__ volatile("dcbst 0,%0; sync; icbi 0,%0; sync; isync" : : "r"(vector_address) : "memory");
}

extern "C" int powerpc_syscall_invoke_test()
{
    if (powerpc_syscall_invoke_vector_end - powerpc_syscall_invoke_vector_start != 1)
        return 0;

    FlatPtr vector_address;
    __asm__ volatile("li %0,3072" : "=r"(vector_address));
    auto* vector = reinterpret_cast<volatile u32*>(vector_address);
    u32 original = *vector;
    *vector = powerpc_syscall_invoke_vector_start[0];
    flush_syscall_vector();

    bool passed = true;
    powerpc_syscall_response = 0x12345670;
    passed &= syscall0(0x70) == powerpc_syscall_response
        && powerpc_syscall_observed[0] == 0x70;

    powerpc_syscall_response = 0x12345671;
    passed &= syscall1(0x71, 0x11u) == powerpc_syscall_response
        && powerpc_syscall_observed[0] == 0x71 && powerpc_syscall_observed[1] == 0x11;

    powerpc_syscall_response = 0x12345672;
    passed &= syscall2(0x72, 0x21u, 0x22u) == powerpc_syscall_response
        && powerpc_syscall_observed[0] == 0x72 && powerpc_syscall_observed[1] == 0x21
        && powerpc_syscall_observed[2] == 0x22;

    powerpc_syscall_response = 0x12345673;
    passed &= syscall3(0x73, 0x31u, 0x32u, 0x33u) == powerpc_syscall_response
        && powerpc_syscall_observed[0] == 0x73 && powerpc_syscall_observed[1] == 0x31
        && powerpc_syscall_observed[2] == 0x32 && powerpc_syscall_observed[3] == 0x33;

    powerpc_syscall_response = 0x12345674;
    passed &= syscall4(0x74, 0x41u, 0x42u, 0x43u, 0x44u) == powerpc_syscall_response
        && powerpc_syscall_observed[0] == 0x74 && powerpc_syscall_observed[1] == 0x41
        && powerpc_syscall_observed[2] == 0x42 && powerpc_syscall_observed[3] == 0x43
        && powerpc_syscall_observed[4] == 0x44;

    *vector = original;
    flush_syscall_vector();
    return passed;
}
