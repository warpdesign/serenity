/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "KernelContextOffsets.h"

.section .text, "ax"
.p2align 2
.globl powerpc_restore_supervisor_exception_state
.type powerpc_restore_supervisor_exception_state,@function
powerpc_restore_supervisor_exception_state:
    // r3 points to a RegisterState. Its saved SRR1 must select supervisor
    // mode. User return needs a real-mode trampoline to change high VSIDs.
    mr 12,3
    lwz 11,PPC_CONTEXT_IP(12)
    mtspr 26,11
    lwz 11,PPC_CONTEXT_SRR1(12)
    mtspr 27,11
    lwz 11,PPC_CONTEXT_CR(12)
    mtcrf 255,11
    lwz 11,PPC_CONTEXT_CTR(12)
    mtctr 11
    lwz 11,PPC_CONTEXT_XER(12)
    mtxer 11
    lwz 11,PPC_CONTEXT_LR(12)
    mtlr 11

    lwz 0,PPC_CONTEXT_GPR0(12)
    lwz 1,PPC_CONTEXT_GPR1(12)
    lwz 2,PPC_CONTEXT_GPR2(12)
    lwz 3,PPC_CONTEXT_GPR3(12)
    lwz 4,16(12)
    lwz 5,20(12)
    lwz 6,24(12)
    lwz 7,28(12)
    lwz 8,32(12)
    lwz 9,36(12)
    lwz 10,40(12)
    lmw 13,PPC_CONTEXT_GPR13(12)
    lwz 11,PPC_CONTEXT_GPR11(12)
    lwz 12,PPC_CONTEXT_GPR12(12)
    rfi
.size powerpc_restore_supervisor_exception_state, .-powerpc_restore_supervisor_exception_state
