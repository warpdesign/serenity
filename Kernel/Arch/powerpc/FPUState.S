/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

.text
.p2align 2
.globl store_fpu_state
.type store_fpu_state,@function
store_fpu_state:
    stfd 0,0(3)
    stfd 1,8(3)
    stfd 2,16(3)
    stfd 3,24(3)
    stfd 4,32(3)
    stfd 5,40(3)
    stfd 6,48(3)
    stfd 7,56(3)
    stfd 8,64(3)
    stfd 9,72(3)
    stfd 10,80(3)
    stfd 11,88(3)
    stfd 12,96(3)
    stfd 13,104(3)
    stfd 14,112(3)
    stfd 15,120(3)
    stfd 16,128(3)
    stfd 17,136(3)
    stfd 18,144(3)
    stfd 19,152(3)
    stfd 20,160(3)
    stfd 21,168(3)
    stfd 22,176(3)
    stfd 23,184(3)
    stfd 24,192(3)
    stfd 25,200(3)
    stfd 26,208(3)
    stfd 27,216(3)
    stfd 28,224(3)
    stfd 29,232(3)
    stfd 30,240(3)
    stfd 31,248(3)
    mffs 0
    stfd 0,256(3)
    lfd 0,0(3)
    blr
.size store_fpu_state, .-store_fpu_state

.p2align 2
.globl load_fpu_state
.type load_fpu_state,@function
load_fpu_state:
    lfd 0,256(3)
    mtfsf 255,0
    lfd 0,0(3)
    lfd 1,8(3)
    lfd 2,16(3)
    lfd 3,24(3)
    lfd 4,32(3)
    lfd 5,40(3)
    lfd 6,48(3)
    lfd 7,56(3)
    lfd 8,64(3)
    lfd 9,72(3)
    lfd 10,80(3)
    lfd 11,88(3)
    lfd 12,96(3)
    lfd 13,104(3)
    lfd 14,112(3)
    lfd 15,120(3)
    lfd 16,128(3)
    lfd 17,136(3)
    lfd 18,144(3)
    lfd 19,152(3)
    lfd 20,160(3)
    lfd 21,168(3)
    lfd 22,176(3)
    lfd 23,184(3)
    lfd 24,192(3)
    lfd 25,200(3)
    lfd 26,208(3)
    lfd 27,216(3)
    lfd 28,224(3)
    lfd 29,232(3)
    lfd 30,240(3)
    lfd 31,248(3)
    blr
.size load_fpu_state, .-load_fpu_state
