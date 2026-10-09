/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

// Copied to the low system-call vector after firmware access is complete.
.section .text.system_call_probe, "ax"
.global system_call_probe_vector_start
.global system_call_probe_vector_end
system_call_probe_vector_start:
    ba powerpc_system_call_probe_handler
system_call_probe_vector_end:

.section .text, "ax"
.type powerpc_system_call_probe_handler, @function
powerpc_system_call_probe_handler:
    // Address translation is off. Save every user GPR before using a scratch
    // register, then capture the rest of the integer exception state.
    mtspr 275, 12
    lis 12, powerpc_user_probe_frame@ha
    addi 12, 12, powerpc_user_probe_frame@l
    stw 0, 0(12)
    stw 1, 4(12)
    stw 2, 8(12)
    stw 3, 12(12)
    stw 4, 16(12)
    stw 5, 20(12)
    stw 6, 24(12)
    stw 7, 28(12)
    stw 8, 32(12)
    stw 9, 36(12)
    stw 10, 40(12)
    stw 11, 44(12)
    mfspr 11, 275
    stw 11, 48(12)
    stmw 13, 52(12)
    mfcr 11
    stw 11, 128(12)
    mfctr 11
    stw 11, 132(12)
    mfxer 11
    stw 11, 136(12)
    mflr 11
    stw 11, 140(12)
    mfspr 11, 26
    stw 11, 144(12)
    mfspr 11, 27
    stw 11, 148(12)

    lis 12, powerpc_user_probe_syscall_count@ha
    lwz 11, powerpc_user_probe_syscall_count@l(12)
    addi 11, 11, 1
    stw 11, powerpc_user_probe_syscall_count@l(12)
    cmpwi 11, 1
    beq .Lreturn_to_user

    // The second sc returns to the supervisor probe.
    lis 12, powerpc_user_probe_saved_msr@ha
    lwz 11, powerpc_user_probe_saved_msr@l(12)
    mtspr 27, 11
    lis 12, powerpc_user_probe_resume_address@ha
    lwz 11, powerpc_user_probe_resume_address@l(12)
    mtspr 26, 11
    lis 12, powerpc_user_probe_saved_supervisor_stack@ha
    lwz 1, powerpc_user_probe_saved_supervisor_stack@l(12)
    rfi

.Lreturn_to_user:
    lis 12, powerpc_user_probe_observed_user_stack@ha
    stw 1, powerpc_user_probe_observed_user_stack@l(12)
    mfspr 11, 26
    lis 12, powerpc_user_probe_observed_srr0@ha
    stw 11, powerpc_user_probe_observed_srr0@l(12)
    mfspr 11, 27
    lis 12, powerpc_user_probe_observed_srr1@ha
    stw 11, powerpc_user_probe_observed_srr1@l(12)
    lis 12, powerpc_user_probe_frame@ha
    addi 12, 12, powerpc_user_probe_frame@l
    lwz 11, 128(12)
    mtcrf 255, 11
    lwz 11, 132(12)
    mtctr 11
    lwz 11, 136(12)
    mtxer 11
    lwz 11, 140(12)
    mtlr 11
    lwz 11, 144(12)
    mtspr 26, 11
    lwz 11, 148(12)
    mtspr 27, 11
    lwz 0, 0(12)
    lwz 1, 4(12)
    lwz 2, 8(12)
    lwz 3, 12(12)
    lwz 4, 16(12)
    lwz 5, 20(12)
    lwz 6, 24(12)
    lwz 7, 28(12)
    lwz 8, 32(12)
    lwz 9, 36(12)
    lwz 10, 40(12)
    lmw 13, 52(12)
    lwz 11, 44(12)
    lwz 12, 48(12)
    rfi
.size powerpc_system_call_probe_handler, .-powerpc_system_call_probe_handler

.global powerpc_enter_user_probe
.type powerpc_enter_user_probe, @function
powerpc_enter_user_probe:
    mflr 0
    mfcr 12
    lis 11, powerpc_user_probe_saved_supervisor_cr@ha
    stw 12, powerpc_user_probe_saved_supervisor_cr@l(11)
    mfmsr 12
    lis 11, powerpc_user_probe_saved_msr@ha
    stw 12, powerpc_user_probe_saved_msr@l(11)
    lis 12, powerpc_user_probe_supervisor_resume@ha
    addi 12, 12, powerpc_user_probe_supervisor_resume@l
    lis 11, powerpc_user_probe_resume_address@ha
    stw 12, powerpc_user_probe_resume_address@l(11)
    mfmsr 12
    ori 12, 12, 0x4030  // PR | IR | DR
    or 12, 12, 8         // Optional EE bit supplied by the caller.
    mtspr 27, 12
    mtspr 26, 3
    lis 11, powerpc_user_probe_saved_supervisor_stack@ha
    stw 1, powerpc_user_probe_saved_supervisor_stack@l(11)
    mr 1, 7
    li 3, 0
    rfi
powerpc_user_probe_supervisor_resume:
    lis 11, powerpc_user_probe_saved_supervisor_cr@ha
    lwz 11, powerpc_user_probe_saved_supervisor_cr@l(11)
    mtcrf 255, 11
    mtlr 0
    blr
.size powerpc_enter_user_probe, .-powerpc_enter_user_probe

// Use a mapped user stack and data page, then fault on the high kernel load.
.section .text.user_mode_probe, "ax"
.balign 4096
.global powerpc_user_probe_start
.global powerpc_user_probe_end
powerpc_user_probe_start:
    stwu 1, -16(1)
    stw 6, 8(1)
    lwz 3, 8(1)
    stw 3, 0(5)
    lwz 3, 0(5)
    lwz 3, 0(4)
    addi 1, 1, 16
    li 8, 8
    li 9, 9
    li 10, 10
    mtctr 10
    li 10, 0
    mtxer 10
    lis 10, 0x1122
    ori 10, 10, 0x3344
    mtlr 10
    li 10, 10
    li 11, 5
    li 12, 7
    cmpw 11, 12
    sc
.global powerpc_user_probe_after_first_syscall
powerpc_user_probe_after_first_syscall:
    stw 8, 4(5)
    stw 9, 8(5)
    stw 10, 12(5)
    stw 11, 16(5)
    stw 12, 20(5)
    mfcr 8
    stw 8, 24(5)
    mfctr 8
    stw 8, 28(5)
    mfxer 8
    stw 8, 32(5)
    mflr 8
    stw 8, 36(5)
    stw 1, 40(5)
    sc
.global powerpc_user_probe_after_second_syscall
powerpc_user_probe_after_second_syscall:
1:
    b 1b
powerpc_user_probe_end:

// A separate user program waits for the high kernel IRQ handler to signal its
// mapped data page, then exits through the existing two-syscall probe path.
.section .text.user_irq_probe, "ax"
.balign 4096
.global powerpc_user_irq_probe_start
.global powerpc_user_irq_probe_end
powerpc_user_irq_probe_start:
    lis 8, 0x0040
    mtctr 8
1:
    lwz 10, 0(5)
    cmpwi 10, 0
    bne 2f
    bdnz 1b
2:
    stw 10, 4(5)
    stw 1, 8(5)
    sc
    sc
3:
    b 3b
powerpc_user_irq_probe_end:
