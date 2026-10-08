/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

.text
.global powerpc_verify_preserved_registers
.type powerpc_verify_preserved_registers, @function
powerpc_verify_preserved_registers:
    stwu 1, -320(1)
    mflr 0
    stw 0, 324(1)
    stw 14, 8(1)
    stfd 14, 16(1)

    li 14, 0x1234
    lis 9, .Lsaved_f14@ha
    addi 9, 9, .Lsaved_f14@l
    lfd 14, 0(9)

    addi 3, 1, 32
    bl setjmp
    cmpwi 3, 0
    bne .Lafter_longjmp

    li 14, 0
    lis 9, .Lzero@ha
    addi 9, 9, .Lzero@l
    lfd 14, 0(9)
    addi 3, 1, 32
    li 4, 11
    bl longjmp
    b .Lfailed

.Lafter_longjmp:
    cmpwi 3, 11
    bne .Lfailed
    cmpwi 14, 0x1234
    bne .Lfailed
    stfd 14, 288(1)
    lwz 9, 288(1)
    lis 10, 0x400a
    cmpw 9, 10
    bne .Lfailed
    lwz 9, 292(1)
    cmpwi 9, 0
    bne .Lfailed
    li 3, 1
    b .Lrestore

.Lfailed:
    li 3, 0
.Lrestore:
    lwz 14, 8(1)
    lfd 14, 16(1)
    lwz 0, 324(1)
    mtlr 0
    addi 1, 1, 320
    blr
.size powerpc_verify_preserved_registers, .-powerpc_verify_preserved_registers

.section .rodata
.balign 8
.Lsaved_f14:
    .quad 0x400a000000000000
.Lzero:
    .quad 0
