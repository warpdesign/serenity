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
    bl .Lbase
.Lbase:
    mflr 31

    // Open the pre-created virtual console. SC_open_params is 20 bytes on PPC32.
    addi 3,1,-32
    li 4,-100                 // AT_FDCWD
    stw 4,0(3)
    addi 4,31,.Ltty_path-.Lbase
    stw 4,4(3)
    li 4,.Ltty_path_end-.Ltty_path
    stw 4,8(3)
    li 4,2                    // O_WRONLY
    stw 4,12(3)
    li 4,0
    sth 4,16(3)
    li 0,SYSCALL_OPEN
    sc
    cmpwi 3,0
    blt .Lfailed

    addi 4,31,.Lmessage-.Lbase
    li 5,.Lmessage_end-.Lmessage
    li 0,SYSCALL_WRITE
    sc
    cmpwi 3,.Lmessage_end-.Lmessage
    bne .Lfailed
.Lwait:
    b .Lwait
.Lfailed:
    tw 31,0,0

.Ltty_path:
    .ascii "/dev/tty0"
.Ltty_path_end:
.Lmessage:
    .ascii "VirtIO GPU color test\n"
.Lmessage_end:
.size _start,.-_start

.section .bss,"aw",@nobits
.p2align 12
.Lscratch:
    .space 64
