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
    // The loader's BAT0 identity mapping kept the low bootstrap stack
    // accessible through early initialization. With r1 now on the HPT-backed
    // high kernel stack, remove that mapping before user processes run.
    li 11,0
    sync
    mtspr 528,11 // IBAT0U
    mtspr 536,11 // DBAT0U
    isync
    lwz 12,PPC_CONTEXT_IP(3)
    mtctr 12
    mr 3,4
    bctr
.size powerpc_start_initial_context, .-powerpc_start_initial_context

.globl do_assume_context
.type do_assume_context,@function
do_assume_context:
    // r3: current Thread, r4: requested interrupt state. Reserve space
    // below the old kernel stack before building a fresh frame above it.
    mr 30,3
    stwu 1,-192(1)
    bl powerpc_prepare_assume_context
    mr 4,30
    b powerpc_start_assumed_context
.size do_assume_context, .-do_assume_context

.globl powerpc_start_assumed_context
.type powerpc_start_assumed_context,@function
powerpc_start_assumed_context:
    // r3: prepared TrapFrame stack pointer, r4: the same Thread as both
    // source and destination. Continue through the normal first entry path.
    mr 30,4
    mr 1,3
    stwu 1,-64(1)
    stw 30,48(1)
    mr 3,30
    mr 4,30
    bl powerpc_enter_thread_context
    lwz 30,48(1)
    lwz 1,0(1)
    mr 3,30
    mr 4,30
    b thread_context_first_enter
.size powerpc_start_assumed_context, .-powerpc_start_assumed_context
