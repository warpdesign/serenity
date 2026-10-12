/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

.section .text,"ax"
.p2align 2
.globl _start
.type _start,@function
_start:
    bl .Lparameters_address
.Lparameters_address:
    mflr 3
    addi 3,3,.Lparameters-.Lparameters_address
    li 0,SYSCALL_MMAP
    sc
    cmpwi 3,-12
    beq .Lfailed

    mr 16,3
    li 17,0x123
    stw 17,0(16)
    lis 18,0x3000
    add 18,16,18
    addi 18,18,-4096
    li 19,0x456
    stw 19,0(18)
    lwz 20,0(16)
    cmpw 20,17
    bne .Lfailed
    lwz 20,0(18)
    cmpw 20,19
    bne .Lfailed

    bl .Lmessage_address
.Lmessage_address:
    mflr 3
    addi 3,3,.Lmessage-.Lmessage_address
    li 4,.Lmessage_end-.Lmessage
    li 0,SYSCALL_DBGPUTSTR
    sc
    cmpwi 3,.Lmessage_end-.Lmessage
    bne .Lfailed
.Lloop:
    b .Lloop
.Lfailed:
    tw 31,0,0

.p2align 3
.Lparameters:
    .long 0 // addr
    .long 0x30000000 // 768 MiB
    .long 0 // alignment
    .long 3 // PROT_READ | PROT_WRITE
    .long 0xa2 // MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE
    .long -1 // fd
    .quad 0 // offset
    .long 0 // name.characters
    .long 0 // name.length
.Lmessage:
    .ascii "PowerPC large shadow mapping passed\n"
.Lmessage_end:
.size _start,.-_start

.section .bss,"aw",@nobits
.p2align 12
.Lscratch:
    .space 64
