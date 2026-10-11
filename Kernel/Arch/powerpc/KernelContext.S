/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "KernelContextOffsets.h"

.section .text, "ax"
.p2align 2
.globl powerpc_switch_kernel_context
.type powerpc_switch_kernel_context,@function
powerpc_switch_kernel_context:
    // r3: outgoing ThreadRegisters, r4: incoming ThreadRegisters,
    // r5: from_thread, r6: to_thread. Preserve the kernel ABI's nonvolatile
    // state. The call's LR is the outgoing continuation address.
    stw 1,PPC_CONTEXT_GPR1(3)
    stw 2,PPC_CONTEXT_GPR2(3)
    stmw 13,PPC_CONTEXT_GPR13(3)
    mfcr 0
    stw 0,PPC_CONTEXT_CR(3)
    mfctr 0
    stw 0,PPC_CONTEXT_CTR(3)
    mfxer 0
    stw 0,PPC_CONTEXT_XER(3)
    mflr 0
    stw 0,PPC_CONTEXT_LR(3)
    stw 0,PPC_CONTEXT_IP(3)

    // Enter the destination thread on its own stack before restoring its
    // nonvolatile registers. Keep the arguments and register block across
    // the C++ call beyond the ABI's eight argument-save slots.
    lwz 1,PPC_CONTEXT_GPR1(4)
    stwu 1,-64(1)
    stw 4,48(1)
    stw 5,52(1)
    stw 6,56(1)
    mr 3,5
    mr 4,6
    bl powerpc_enter_thread_context
    lwz 12,48(1)
    lwz 5,52(1)
    lwz 6,56(1)
    lwz 1,0(1)

    lwz 0,PPC_CONTEXT_CR(12)
    mtcrf 255,0
    lwz 0,PPC_CONTEXT_XER(12)
    mtxer 0
    lwz 0,PPC_CONTEXT_LR(12)
    mtlr 0
    lwz 0,PPC_CONTEXT_IP(12)
    mtctr 0
    lmw 13,PPC_CONTEXT_GPR13(12)
    lwz 2,PPC_CONTEXT_GPR2(12)
    mr 3,5
    mr 4,6
    // CTR is caller-volatile in the kernel C ABI and holds the branch target.
    // Exception return restores the separate user-mode CTR from the trap frame.
    bctr
.size powerpc_switch_kernel_context, .-powerpc_switch_kernel_context
