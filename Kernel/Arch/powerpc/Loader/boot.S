/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

.section .text.first, "ax"
.globl _start
.type _start,@function
_start:
    lis 6,__bss_start@ha
    addi 6,6,__bss_start@l
    lis 7,__bss_end@ha
    addi 7,7,__bss_end@l
    li 8,0
.Lclear_bss:
    cmplw 6,7
    bge .Lbss_done
    stw 8,0(6)
    addi 6,6,4
    b .Lclear_bss
.Lbss_done:
    // A help request returns to the caller. Save its ABI state before using
    // the loader stack and the nonvolatile registers for firmware arguments.
    lis 6,powerpc_loader_entry_state@ha
    addi 6,6,powerpc_loader_entry_state@l
    stw 1,0(6)
    stw 29,4(6)
    stw 30,8(6)
    mflr 8
    stw 8,12(6)
    mfmsr 8
    stw 8,16(6)
    // Open Firmware supplies its client entry in r5.
    mr 30,5
    // Keep the MSR the firmware entered with, for the loader's diagnostics.
    mfmsr 29
    mfmsr 9
    ori 9,9,0x2000 // The kernel initializes and saves scalar FP state.
    mtmsr 9
    isync
    lis 1,__stack_end@ha
    addi 1,1,__stack_end@l
    addi 1,1,-16
    li 0,0
    stw 0,0(1)
    mr 3,30
    mr 4,29
    bl loader_main
    lis 6,powerpc_loader_return_to_firmware@ha
    lwz 6,powerpc_loader_return_to_firmware@l(6)
    cmpwi 6,0
    beq .Lhalt
    lis 6,powerpc_loader_entry_state@ha
    addi 6,6,powerpc_loader_entry_state@l
    lwz 8,12(6)
    mtlr 8
    lwz 9,16(6)
    lwz 29,4(6)
    lwz 30,8(6)
    lwz 1,0(6)
    li 3,0
    sync
    mtmsr 9
    isync
    blr
.Lhalt:
    b .Lhalt
.size _start,.-_start

// int powerpc_loader_call_firmware_real_mode(void* arguments, int (*entry)(void*));
// The PowerPC Open Firmware binding requires a real-mode firmware to be entered with IR and DR clear.
// The loader, its stack and everything passed to the firmware are identity-addressed, so execution
// continues at the same addresses when translation is turned off around the call.
.globl powerpc_loader_call_firmware_real_mode
.type powerpc_loader_call_firmware_real_mode,@function
powerpc_loader_call_firmware_real_mode:
    stwu 1,-32(1)
    mflr 0
    stw 0,36(1)
    mfmsr 11
    stw 11,24(1)
    li 0,0x30
    andc 11,11,0
    sync
    mtmsr 11
    isync
    mtctr 4
    bctrl
    lwz 11,24(1)
    sync
    mtmsr 11
    isync
    lwz 0,36(1)
    mtlr 0
    addi 1,1,32
    blr
.size powerpc_loader_call_firmware_real_mode,.-powerpc_loader_call_firmware_real_mode

.section .bss
.balign 4
powerpc_loader_entry_state:
    .space 20
.globl powerpc_loader_return_to_firmware
powerpc_loader_return_to_firmware:
    .long 0
