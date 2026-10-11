/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

.section .text, "ax"
.p2align 2
.globl _start
.type _start,@function
_start:
    addi 1,1,-64
    li 0,0
    stw 0,0(1)
    mtlr 0
    b _entry
.size _start,.-_start
