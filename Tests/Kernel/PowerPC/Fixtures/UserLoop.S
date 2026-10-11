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
    // The second page is mapped from the executable on its first fetch.
    b .Lsecond_page
.size _start,.-_start

.section .text.second,"ax"
.p2align 2
.Lsecond_page:
    // Touch a lower stack page so user-mode DSI must return successfully.
    addi 3,1,-8192
    li 4,0x55
    stw 4,0(3)
    li 0,SYSCALL_GETPID
    sc
    cmpwi 3,0
    ble .Lsyscall_failed
    bl .Lmessage_address
.Lmessage_address:
    mflr 3
    addi 3,3,.Lmessage-.Lmessage_address
    lbz 5,0(3)
    cmpwi 5,'P'
    bne .Lsyscall_failed
    li 4,.Lmessage_end-.Lmessage
    li 0,SYSCALL_DBGPUTSTR
    sc
    cmpwi 3,.Lmessage_end-.Lmessage
    bne .Lsyscall_failed
.Lloop:
    b .Lloop
.Lsyscall_failed:
    tw 31,0,0
.Lmessage:
    .ascii "PPC user syscall\n"
.Lmessage_end:
