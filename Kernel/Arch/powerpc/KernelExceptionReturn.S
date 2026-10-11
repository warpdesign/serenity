/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "KernelContextOffsets.h"
#include "HighSegmentSwitch.inc"

.macro POWERPC_RESTORE_EXCEPTION_FRAME
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
.endm

.section .text, "ax"
.p2align 2
.globl powerpc_restore_supervisor_exception_state
.type powerpc_restore_supervisor_exception_state,@function
powerpc_restore_supervisor_exception_state:
    // r3 points to a RegisterState. Its saved SRR1 must select supervisor
    // mode. User return needs a real-mode trampoline to change high VSIDs.
    mr 12,3
    POWERPC_RESTORE_EXCEPTION_FRAME
.size powerpc_restore_supervisor_exception_state, .-powerpc_restore_supervisor_exception_state

.globl powerpc_restore_user_exception_state_real_mode
.type powerpc_restore_user_exception_state_real_mode,@function
powerpc_restore_user_exception_state_real_mode:
    // r3 is the physical address of a RegisterState. Both instruction and
    // data translation must be off before replacing the high kernel VSIDs.
    mr 12,3
    mfmsr 11
    andi. 11,11,0x30
    bne .Linvalid_user_return
    lwz 11,PPC_CONTEXT_SRR1(12)
    andi. 11,11,0x4030
    cmplwi 11,0x4030
    bne .Linvalid_user_return
    // SR1 must be Kp=1 with VSID base+1 from a user 16-VSID block.
    // Rejecting the reserved low kernel VSIDs keeps read-only high kernel
    // HPT entries inaccessible after the segment switch.
    mfsr 11,1
    srwi 10,11,24
    cmplwi 10,0x20
    bne .Linvalid_user_return
    clrlwi 10,11,8
    andi. 0,10,15
    cmpwi 0,1
    bne .Linvalid_user_return
    cmplwi 10,0x101
    blt .Linvalid_user_return
    POWERPC_USE_USER_HIGH_SEGMENTS 11
    POWERPC_RESTORE_EXCEPTION_FRAME
.Linvalid_user_return:
    tw 31,0,0
    b .Linvalid_user_return
.size powerpc_restore_user_exception_state_real_mode, .-powerpc_restore_user_exception_state_real_mode

.globl powerpc_restore_user_exception_state_from_high_real_mode
.type powerpc_restore_user_exception_state_from_high_real_mode,@function
powerpc_restore_user_exception_state_from_high_real_mode:
    // The syscall vector entered in real mode with r3 pointing to a high
    // kernel-mapped RegisterState. Keep IR off and enable DR only long enough
    // to read that frame under the shared kernel high VSIDs.
    mr 12,3
    mfmsr 11
    andi. 11,11,0x30
    bne .Linvalid_high_user_return
    mfsr 11,1
    srwi 10,11,24
    cmplwi 10,0x20
    bne .Linvalid_high_user_return
    clrlwi 10,11,8
    andi. 0,10,15
    cmpwi 0,1
    bne .Linvalid_high_user_return
    cmplwi 10,0x101
    blt .Linvalid_high_user_return

    mfmsr 11
    ori 11,11,0x10
    sync
    mtmsr 11
    isync
    lwz 11,PPC_CONTEXT_SRR1(12)
    andi. 11,11,0x4030
    cmplwi 11,0x4030
    bne .Linvalid_high_user_return

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
    // SPRG2/3 hold the last two user GPRs needed after DR is disabled.
    // Exception entry uses these as scratch registers, so no persistent
    // processor state is displaced.
    lwz 11,PPC_CONTEXT_GPR11(12)
    mtspr 274,11
    lwz 11,PPC_CONTEXT_GPR12(12)
    mtspr 275,11
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

    mfmsr 11
    li 12,-17
    and 11,11,12
    sync
    mtmsr 11
    isync
    POWERPC_USE_USER_HIGH_SEGMENTS 11
    mfspr 11,274
    mfspr 12,275
    rfi
.Linvalid_high_user_return:
    tw 31,0,0
    b .Linvalid_high_user_return
.size powerpc_restore_user_exception_state_from_high_real_mode, .-powerpc_restore_user_exception_state_from_high_real_mode
