/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

.section .text.safemem, "ax"
.p2align 2

.globl powerpc_safe_memcpy
.type powerpc_safe_memcpy,@function
powerpc_safe_memcpy:
    // r3: destination, r4: source, r5: count, r6: &fault_at.
    cmpwi 5,0
    beq 2f
    mtctr 5
1:
.globl safe_memcpy_ins_1
safe_memcpy_ins_1:
    lbz 7,0(4)
.globl safe_memcpy_ins_2
safe_memcpy_ins_2:
    stb 7,0(3)
    addi 3,3,1
    addi 4,4,1
    bdnz 1b
2:
    li 3,1
.globl safe_memcpy_faulted
safe_memcpy_faulted:
    blr
.size powerpc_safe_memcpy, .-powerpc_safe_memcpy

.p2align 2
.globl powerpc_safe_memset
.type powerpc_safe_memset,@function
powerpc_safe_memset:
    // r3: destination, r4: byte, r5: count, r6: &fault_at.
    cmpwi 5,0
    beq 2f
    mtctr 5
1:
.globl safe_memset_ins
safe_memset_ins:
    stb 4,0(3)
    addi 3,3,1
    bdnz 1b
2:
    li 3,1
.globl safe_memset_faulted
safe_memset_faulted:
    blr
.size powerpc_safe_memset, .-powerpc_safe_memset

.p2align 2
.globl powerpc_safe_strnlen
.type powerpc_safe_strnlen,@function
powerpc_safe_strnlen:
    // r3: string, r4: maximum length, r5: &fault_at.
    mr 6,3
    li 3,0
    cmpwi 4,0
    beq 2f
1:
    add 7,6,3
.globl safe_strnlen_ins
safe_strnlen_ins:
    lbz 8,0(7)
    cmpwi 8,0
    beq 2f
    addi 3,3,1
    cmplw 3,4
    blt 1b
2:
.globl safe_strnlen_faulted
safe_strnlen_faulted:
    blr
.size powerpc_safe_strnlen, .-powerpc_safe_strnlen
