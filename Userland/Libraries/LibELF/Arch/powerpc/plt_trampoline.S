/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

.p2align 2
.globl _plt_trampoline
.hidden _plt_trampoline
.type _plt_trampoline,@function

/* Secure PLT glink supplies DynamicObject* in r12 and the .rela.plt byte
 * offset in r11. Keep the original call arguments while resolving the slot. */
_plt_trampoline:
    stwu 1,-192(1)
    stw 0,8(1)
    mflr 0
    stw 0,12(1)
    mfcr 0
    stw 0,16(1)

    stw 3,20(1)
    stw 4,24(1)
    stw 5,28(1)
    stw 6,32(1)
    stw 7,36(1)
    stw 8,40(1)
    stw 9,44(1)
    stw 10,48(1)

    stfd 1,56(1)
    stfd 2,64(1)
    stfd 3,72(1)
    stfd 4,80(1)
    stfd 5,88(1)
    stfd 6,96(1)
    stfd 7,104(1)
    stfd 8,112(1)
    stfd 9,120(1)
    stfd 10,128(1)
    stfd 11,136(1)
    stfd 12,144(1)
    stfd 13,152(1)

    mr 3,12
    mr 4,11
    bl _fixup_plt_entry
    mtctr 3

    lfd 1,56(1)
    lfd 2,64(1)
    lfd 3,72(1)
    lfd 4,80(1)
    lfd 5,88(1)
    lfd 6,96(1)
    lfd 7,104(1)
    lfd 8,112(1)
    lfd 9,120(1)
    lfd 10,128(1)
    lfd 11,136(1)
    lfd 12,144(1)
    lfd 13,152(1)

    lwz 3,20(1)
    lwz 4,24(1)
    lwz 5,28(1)
    lwz 6,32(1)
    lwz 7,36(1)
    lwz 8,40(1)
    lwz 9,44(1)
    lwz 10,48(1)

    lwz 0,16(1)
    mtcrf 0xff,0
    lwz 0,12(1)
    mtlr 0
    lwz 0,8(1)
    addi 1,1,192
    bctr

.size _plt_trampoline, .-_plt_trampoline
