/*
 * Copyright (c) 2020-2026, Itamar S. <itamar8910@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

.section .text
.globl _start
.type _start,@function
_start:
    push $0
    jmp _entry@plt
.size _start,.-_start
