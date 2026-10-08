/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

.section .text.first, "ax"
.global _start
.type _start, @function
_start:
    // Open Firmware passes its client interface in r5. Keep it across BSS setup.
    mr 30, 5

    lis 6, __bss_start@ha
    addi 6, 6, __bss_start@l
    lis 7, __bss_end@ha
    addi 7, 7, __bss_end@l
    li 8, 0
.Lclear_bss:
    cmplw 6, 7
    bge .Lbss_done
    stw 8, 0(6)
    addi 6, 6, 4
    b .Lclear_bss
.Lbss_done:
    // The QEMU firmware enters with FP disabled. The libc context test must
    // be able to save and restore the nonvolatile floating-point registers.
    mfmsr 9
    ori 9, 9, 0x2000
    mtmsr 9
    isync

    lis 1, __stack_end@ha
    addi 1, 1, __stack_end@l
    addi 1, 1, -16
    li 0, 0
    stw 0, 0(1)

    mr 3, 30
    bl boot_main
.Lhalt:
    b .Lhalt
.size _start, .-_start
