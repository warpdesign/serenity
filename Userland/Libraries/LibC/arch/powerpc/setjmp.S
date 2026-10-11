/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

// Keep these offsets in sync with struct __jmp_buf in setjmp.h.
#define FPR_SLOT 96
#define DID_SAVE_SIGNAL_MASK_SLOT 240
#define SAVED_SIGNAL_MASK_SLOT 244

.text
.global _setjmp
.global setjmp
.global sigsetjmp
.type _setjmp, @function
.type setjmp, @function
.type sigsetjmp, @function
_setjmp:
setjmp:
    li 4, 0
sigsetjmp:
    stw 4, DID_SAVE_SIGNAL_MASK_SLOT(3)
    li 0, 0
    stw 0, SAVED_SIGNAL_MASK_SLOT(3)
    cmpwi 4, 0
    beq .Lsave_registers

    // r3 and LR must survive the optional sigprocmask call.
    stwu 1, -64(1)
    mflr 0
    stw 0, 68(1)
    stw 3, 48(1)
    addi 5, 3, SAVED_SIGNAL_MASK_SLOT
    li 4, 0
    li 3, 0
    bl sigprocmask
    lwz 3, 48(1)
    lwz 0, 68(1)
    mtlr 0
    addi 1, 1, 64

.Lsave_registers:
    stw 1, 0(3)
    stw 2, 4(3)
    stw 13, 8(3)
    stw 14, 12(3)
    stw 15, 16(3)
    stw 16, 20(3)
    stw 17, 24(3)
    stw 18, 28(3)
    stw 19, 32(3)
    stw 20, 36(3)
    stw 21, 40(3)
    stw 22, 44(3)
    stw 23, 48(3)
    stw 24, 52(3)
    stw 25, 56(3)
    stw 26, 60(3)
    stw 27, 64(3)
    stw 28, 68(3)
    stw 29, 72(3)
    stw 30, 76(3)
    stw 31, 80(3)
    mflr 0
    stw 0, 84(3)
    mfcr 0
    stw 0, 88(3)

    stfd 14, FPR_SLOT + 0 * 8(3)
    stfd 15, FPR_SLOT + 1 * 8(3)
    stfd 16, FPR_SLOT + 2 * 8(3)
    stfd 17, FPR_SLOT + 3 * 8(3)
    stfd 18, FPR_SLOT + 4 * 8(3)
    stfd 19, FPR_SLOT + 5 * 8(3)
    stfd 20, FPR_SLOT + 6 * 8(3)
    stfd 21, FPR_SLOT + 7 * 8(3)
    stfd 22, FPR_SLOT + 8 * 8(3)
    stfd 23, FPR_SLOT + 9 * 8(3)
    stfd 24, FPR_SLOT + 10 * 8(3)
    stfd 25, FPR_SLOT + 11 * 8(3)
    stfd 26, FPR_SLOT + 12 * 8(3)
    stfd 27, FPR_SLOT + 13 * 8(3)
    stfd 28, FPR_SLOT + 14 * 8(3)
    stfd 29, FPR_SLOT + 15 * 8(3)
    stfd 30, FPR_SLOT + 16 * 8(3)
    stfd 31, FPR_SLOT + 17 * 8(3)

    li 3, 0
    blr

.global _longjmp
.global longjmp
.type _longjmp, @function
.type longjmp, @function
_longjmp:
longjmp:
    lfd 14, FPR_SLOT + 0 * 8(3)
    lfd 15, FPR_SLOT + 1 * 8(3)
    lfd 16, FPR_SLOT + 2 * 8(3)
    lfd 17, FPR_SLOT + 3 * 8(3)
    lfd 18, FPR_SLOT + 4 * 8(3)
    lfd 19, FPR_SLOT + 5 * 8(3)
    lfd 20, FPR_SLOT + 6 * 8(3)
    lfd 21, FPR_SLOT + 7 * 8(3)
    lfd 22, FPR_SLOT + 8 * 8(3)
    lfd 23, FPR_SLOT + 9 * 8(3)
    lfd 24, FPR_SLOT + 10 * 8(3)
    lfd 25, FPR_SLOT + 11 * 8(3)
    lfd 26, FPR_SLOT + 12 * 8(3)
    lfd 27, FPR_SLOT + 13 * 8(3)
    lfd 28, FPR_SLOT + 14 * 8(3)
    lfd 29, FPR_SLOT + 15 * 8(3)
    lfd 30, FPR_SLOT + 16 * 8(3)
    lfd 31, FPR_SLOT + 17 * 8(3)

    lwz 2, 4(3)
    lwz 13, 8(3)
    lwz 14, 12(3)
    lwz 15, 16(3)
    lwz 16, 20(3)
    lwz 17, 24(3)
    lwz 18, 28(3)
    lwz 19, 32(3)
    lwz 20, 36(3)
    lwz 21, 40(3)
    lwz 22, 44(3)
    lwz 23, 48(3)
    lwz 24, 52(3)
    lwz 25, 56(3)
    lwz 26, 60(3)
    lwz 27, 64(3)
    lwz 28, 68(3)
    lwz 29, 72(3)
    lwz 30, 76(3)
    lwz 31, 80(3)
    lwz 0, 84(3)
    mtlr 0
    lwz 0, 88(3)
    mtcr 0
    lwz 1, 0(3)

    mr 3, 4
    cmpwi 3, 0
    bne .Lreturn
    li 3, 1
.Lreturn:
    blr
