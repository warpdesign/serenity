/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "../../../Kernel/Arch/powerpc/KernelContextOffsets.h"

.section .text, "ax"
.p2align 2
.globl powerpc_supervisor_exception_return_test
.type powerpc_supervisor_exception_return_test,@function
powerpc_supervisor_exception_return_test:
    mflr 0
    stwu 1,-256(1)
    stw 0,8(1)
    stw 14,12(1)
    stw 2,16(1)
    addi 12,1,64

    stw 0,PPC_CONTEXT_GPR0(12)
    stw 1,PPC_CONTEXT_GPR1(12)
    stw 2,PPC_CONTEXT_GPR2(12)
    li 0,0
    stw 0,16(12)
    stw 0,20(12)
    stw 0,24(12)
    stw 0,28(12)
    stw 0,32(12)
    stw 0,36(12)
    stw 0,40(12)
    stw 0,PPC_CONTEXT_GPR11(12)
    stw 0,PPC_CONTEXT_GPR12(12)
    stmw 13,PPC_CONTEXT_GPR13(12)

    lis 0,0x1357
    ori 0,0,0x2468
    stw 0,PPC_CONTEXT_GPR3(12)
    lis 0,0x2468
    ori 0,0,0x1357
    stw 0,56(12) // r14
    lis 0,0x2222
    ori 0,0,0x2222
    stw 0,PPC_CONTEXT_CR(12)
    lis 0,0x5566
    ori 0,0,0x7788
    stw 0,PPC_CONTEXT_CTR(12)
    lis 0,0x2000
    stw 0,PPC_CONTEXT_XER(12)
    lwz 0,8(1)
    stw 0,PPC_CONTEXT_LR(12)
    lis 11,.Lresumed@ha
    addi 11,11,.Lresumed@l
    stw 11,PPC_CONTEXT_IP(12)
    mfmsr 0
    stw 0,PPC_CONTEXT_SRR1(12)

    mr 3,12
    bl powerpc_restore_supervisor_exception_state

.Lresumed:
    mfcr 5
    mfctr 6
    mfxer 7
    lis 4,0x1357
    ori 4,4,0x2468
    cmpw 3,4
    bne .Lfailed
    lis 4,0x2468
    ori 4,4,0x1357
    cmpw 14,4
    bne .Lfailed
    lis 4,0x2222
    ori 4,4,0x2222
    cmpw 5,4
    bne .Lfailed
    lis 4,0x5566
    ori 4,4,0x7788
    cmpw 6,4
    bne .Lfailed
    lis 4,0x2000
    cmpw 7,4
    bne .Lfailed
    lwz 4,16(1)
    cmpw 2,4
    bne .Lfailed
    li 3,1
    b .Ldone
.Lfailed:
    li 3,0
.Ldone:
    lwz 14,12(1)
    lwz 0,8(1)
    mtlr 0
    addi 1,1,256
    blr
.size powerpc_supervisor_exception_return_test, .-powerpc_supervisor_exception_return_test
