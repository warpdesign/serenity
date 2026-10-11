/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

.section .text, "ax"
.p2align 2
.globl powerpc_rtas_enter
.type powerpc_rtas_enter,@function
powerpc_rtas_enter:
    // r3: physical arguments, r4: physical entry, r5: physical return,
    // r6: translated return. Keep only r31 and LR across the firmware call.
    stwu 1,-64(1)
    mflr 0
    stw 0,68(1)
    mfmsr 0
    stw 0,8(1)
    stw 31,12(1)
    mr 31,6
    mtlr 5
    li 0,0x1002 // Machine check and recoverable exception, no translation.
    mtspr 26,4
    mtspr 27,0
    sync
    rfi

.globl powerpc_rtas_return_real
powerpc_rtas_return_real:
    // Firmware returns here in real mode. Avoid memory and stack access.
    mtspr 26,31
    li 0,0x1032 // Restore kernel translations before touching the stack.
    mtspr 27,0
    rfi

.globl powerpc_rtas_return_virtual
powerpc_rtas_return_virtual:
    lwz 0,8(1)
    sync
    mtmsr 0
    isync
    lwz 31,12(1)
    lwz 0,68(1)
    mtlr 0
    addi 1,1,64
    blr
.size powerpc_rtas_enter, .-powerpc_rtas_enter
