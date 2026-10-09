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
    // The first sc returns to user mode. Preserve the registers used below.
    mtspr 275, 12
    lis 12, powerpc_user_probe_saved_user_r11@ha
    stw 11, powerpc_user_probe_saved_user_r11@l(12)
    mfspr 11, 275
    lis 12, powerpc_user_probe_saved_user_r12@ha
    stw 11, powerpc_user_probe_saved_user_r12@l(12)
    mfcr 11
    lis 12, powerpc_user_probe_saved_user_cr@ha
    stw 11, powerpc_user_probe_saved_user_cr@l(12)
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
    lis 12, powerpc_user_probe_saved_user_cr@ha
    lwz 11, powerpc_user_probe_saved_user_cr@l(12)
    mtcrf 255, 11
    lis 12, powerpc_user_probe_saved_user_r11@ha
    lwz 11, powerpc_user_probe_saved_user_r11@l(12)
    lis 12, powerpc_user_probe_saved_user_r12@ha
    lwz 12, powerpc_user_probe_saved_user_r12@l(12)
    rfi
system_call_probe_vector_end:

.section .text, "ax"
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
    li 11, 5
    li 12, 7
    cmpw 11, 12
    sc
    mfcr 8
    stw 8, 4(5)
    stw 11, 8(5)
    stw 12, 12(5)
    stw 1, 16(5)
    sc
1:
    b 1b
powerpc_user_probe_end:
