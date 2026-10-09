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
    mfspr 11, 26
    lis 12, powerpc_user_probe_observed_srr0@ha
    stw 11, powerpc_user_probe_observed_srr0@l(12)
    mfspr 11, 27
    lis 12, powerpc_user_probe_observed_srr1@ha
    stw 11, powerpc_user_probe_observed_srr1@l(12)
    lis 12, powerpc_user_probe_saved_msr@ha
    lwz 11, powerpc_user_probe_saved_msr@l(12)
    mtspr 27, 11
    lis 12, powerpc_user_probe_resume_address@ha
    lwz 11, powerpc_user_probe_resume_address@l(12)
    mtspr 26, 11
    rfi
system_call_probe_vector_end:

.section .text, "ax"
.global powerpc_enter_user_probe
.type powerpc_enter_user_probe, @function
powerpc_enter_user_probe:
    mflr 0
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
    li 3, 0
    rfi
powerpc_user_probe_supervisor_resume:
    mtlr 0
    blr
.size powerpc_enter_user_probe, .-powerpc_enter_user_probe

// The probe uses no user stack yet. It writes and reads a user data page,
// then the high kernel load must fault and be skipped before sc returns.
.section .text.user_mode_probe, "ax"
.balign 4096
.global powerpc_user_probe_start
.global powerpc_user_probe_end
powerpc_user_probe_start:
    stw 6, 0(5)
    lwz 3, 0(5)
    lwz 3, 0(4)
    sc
1:
    b 1b
powerpc_user_probe_end:
