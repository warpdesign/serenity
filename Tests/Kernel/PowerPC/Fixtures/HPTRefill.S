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
    mflr 16
    addi 16,16,.Lparameters-.Lparameters_address
    li 3,4096
    stw 3,4(16)
    li 3,3
    stw 3,12(16)
    li 3,0xb2
    stw 3,16(16)
    li 3,-1
    stw 3,20(16)
    bl .Lstart_message_address
.Lstart_message_address:
    mflr 3
    addi 3,3,.Lstart_message-.Lstart_message_address
    li 4,.Lstart_message_end-.Lstart_message
    li 0,SYSCALL_DBGPUTSTR
    sc

    li 17,0
.Lmap_pages:
    bl .Lcalculate_address
    stw 23,0(16)
    mr 3,16
    li 0,SYSCALL_MMAP
    sc
    cmpw 3,23
    bne .Lfailed
    cmpwi 17,0
    bne .Lwrite_page
    bl .Lfirst_map_message_address
.Lfirst_map_message_address:
    mflr 3
    addi 3,3,.Lfirst_map_message-.Lfirst_map_message_address
    li 4,.Lfirst_map_message_end-.Lfirst_map_message
    li 0,SYSCALL_DBGPUTSTR
    sc
.Lwrite_page:
    addi 24,17,0x100
    stw 24,0(23)
    cmpwi 17,0
    bne .Ladvance_page
    bl .Lfirst_write_message_address
.Lfirst_write_message_address:
    mflr 3
    addi 3,3,.Lfirst_write_message-.Lfirst_write_message_address
    li 4,.Lfirst_write_message_end-.Lfirst_write_message
    li 0,SYSCALL_DBGPUTSTR
    sc
.Ladvance_page:
    addi 17,17,1
    cmpwi 17,16
    bne .Lnext_map
    bl .Lsixteen_message_address
.Lsixteen_message_address:
    mflr 3
    addi 3,3,.Lsixteen_message-.Lsixteen_message_address
    li 4,.Lsixteen_message_end-.Lsixteen_message
    li 0,SYSCALL_DBGPUTSTR
    sc
.Lnext_map:
    cmpwi 17,17
    blt .Lmap_pages

    li 17,0
.Lcheck_pages:
    bl .Lcalculate_address
    lwz 24,0(23)
    addi 25,17,0x100
    cmpw 24,25
    bne .Lfailed
    addi 17,17,1
    cmpwi 17,17
    blt .Lcheck_pages

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

.Lcalculate_address:
    // The user VSID block is 16-aligned. One page in each segment hashes to
    // group G; five more hash to ~G, overfilling the 16 hardware slots.
    cmpwi 17,12
    blt .Lprimary_group_address
    addi 18,17,-12
    xori 20,18,0xedcb
    b .Laddress_page_index_ready
.Lprimary_group_address:
    mr 18,17
    xori 20,18,0x1234
.Laddress_page_index_ready:
    slwi 20,20,12
    slwi 22,18,28
    or 23,20,22
    blr

.Lfailed:
    bl .Lfailure_message_address
.Lfailure_message_address:
    mflr 3
    addi 3,3,.Lfailure_message-.Lfailure_message_address
    li 4,.Lfailure_message_end-.Lfailure_message
    li 0,SYSCALL_DBGPUTSTR
    sc
    tw 31,0,0

.Lstart_message:
    .ascii "PowerPC HPT refill fixture started\n"
.Lstart_message_end:
.Lfirst_map_message:
    .ascii "PowerPC HPT refill first mmap returned\n"
.Lfirst_map_message_end:
.Lfirst_write_message:
    .ascii "PowerPC HPT refill first page written\n"
.Lfirst_write_message_end:
.Lsixteen_message:
    .ascii "PowerPC HPT refill sixteen pages written\n"
.Lsixteen_message_end:
.Lfailure_message:
    .ascii "PowerPC HPT refill fixture failed\n"
.Lfailure_message_end:
.Lmessage:
    .ascii "PowerPC HPT eviction and refill passed\n"
.Lmessage_end:
.size _start,.-_start

.section .bss,"aw",@nobits
.p2align 12
.Lparameters:
    .space 64
