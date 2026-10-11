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
    cmpwi 3,-12 // ENOMEM: the HPT cannot hold this many mappings.
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
    .ascii "PPC mapping failure recovered\n"
.Lmessage_end:
.size _start,.-_start

.section .bss,"aw",@nobits
.p2align 12
.Lscratch:
    .space 64
