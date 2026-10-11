/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "KernelContextOffsets.h"

.section .text, "ax"
.p2align 2
.globl thread_context_first_enter
.type thread_context_first_enter,@function
thread_context_first_enter:
    // r1 points at the new thread's TrapFrame; r3/r4 name the threads.
    // Reserve the ABI linkage and argument-save area for the C++ call.
    stwu 1,-64(1)
    bl context_first_init
    lwz 3,0(1)
    b powerpc_finish_first_thread_context
.size thread_context_first_enter, .-thread_context_first_enter

.globl powerpc_start_initial_context
.type powerpc_start_initial_context,@function
powerpc_start_initial_context:
    // r3: prepared ThreadRegisters, r4: initial Thread.
    lwz 1,PPC_CONTEXT_GPR1(3)
    lwz 2,PPC_CONTEXT_GPR2(3)
    lwz 12,PPC_CONTEXT_IP(3)
    mtctr 12
    mr 3,4
    bctr
.size powerpc_start_initial_context, .-powerpc_start_initial_context
