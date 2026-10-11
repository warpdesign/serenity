/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

.section .text.first, "ax"
.globl _start
.type _start,@function
_start:
    // Open Firmware supplies its client entry in r5.
    mr 30,5
    lis 6,__bss_start@ha
    addi 6,6,__bss_start@l
    lis 7,__bss_end@ha
    addi 7,7,__bss_end@l
    li 8,0
.Lclear_bss:
    cmplw 6,7
    bge .Lbss_done
    stw 8,0(6)
    addi 6,6,4
    b .Lclear_bss
.Lbss_done:
    lis 1,__stack_end@ha
    addi 1,1,__stack_end@l
    addi 1,1,-16
    li 0,0
    stw 0,0(1)
    mr 3,30
    bl loader_main
.Lhalt:
    b .Lhalt
.size _start,.-_start
