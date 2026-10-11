/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

.section .text.start,"ax"
.p2align 2
.globl _start
.type _start,@function
_start:
    b _start
.size _start,.-_start

.section .bss,"aw",@nobits
.p2align 12
.Lscratch:
    .space 64
