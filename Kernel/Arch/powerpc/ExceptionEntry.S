/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "HighSegmentSwitch.inc"
#include "KernelContextOffsets.h"

// Copied into the low vector slots after Open Firmware is finished. Each
// vector uses its own physical stack top in an SPRG and enters the high kernel
// with an exact RegisterState frame.
.macro POWERPC_SAVE_AND_ENTER stack_spr, handler_offset=252
    mtspr 273,0
    mtspr 274,1
    mfspr 1,\stack_spr
    addi 1,1,-256
    stmw 2,PPC_CONTEXT_GPR2(1)
    mfspr 0,273
    stw 0,PPC_CONTEXT_GPR0(1)
    mfspr 0,274
    stw 0,PPC_CONTEXT_GPR1(1)
    mfcr 0
    stw 0,PPC_CONTEXT_CR(1)
    mfctr 0
    stw 0,PPC_CONTEXT_CTR(1)
    mfxer 0
    stw 0,PPC_CONTEXT_XER(1)
    mflr 0
    stw 0,PPC_CONTEXT_LR(1)
    mfspr 0,26
    stw 0,PPC_CONTEXT_IP(1)
    mfspr 0,27
    stw 0,PPC_CONTEXT_SRR1(1)
    mfspr 0,19
    stw 0,152(1)
    mfspr 0,18
    stw 0,156(1)

    POWERPC_USE_KERNEL_HIGH_SEGMENTS 12
    lwz 12,\handler_offset(1)
    // The frame was saved through its physical alias with translation off.
    // C++ exception handlers must use the high kernel alias of that stack.
    addis 1,1,-16384
    mtspr 26,12
    mfmsr 12
    ori 12,12,0x2032 // FP | IR | DR | recoverable, with interrupts disabled.
    mtspr 27,12
    rfi
.endm

// Fault handlers may wait for a file-backed page, and interrupt handlers may
// schedule another thread. Keep their RegisterState and C++ call stack on the
// current thread's kernel stack. Supervisor exceptions continue below the
// interrupted kernel frame; user exceptions begin at the stack top.
.macro POWERPC_MOVE_EXCEPTION_FRAME_TO_THREAD_STACK
    mr 30,1
    lwz 0,PPC_CONTEXT_SRR1(30)
    andi. 0,0,0x4000
    beq .Lsupervisor_stack\@
    stwu 1,-64(1)
    bl powerpc_current_kernel_stack_top
    b .Lhave_stack\@
.Lsupervisor_stack\@:
    lwz 3,PPC_CONTEXT_GPR1(30)
.Lhave_stack\@:
    addi 1,3,-320
    mr 31,1
    mr 10,30
    mr 8,1
    li 9,64
    mtctr 9
.Lcopy_exception_frame\@:
    lwz 9,0(10)
    stw 9,0(8)
    addi 10,10,4
    addi 8,8,4
    bdnz .Lcopy_exception_frame\@
.endm

.section .text.powerpc_data_storage_vector, "ax"
.p2align 2
.globl powerpc_data_storage_vector_start
.globl powerpc_data_storage_vector_end
powerpc_data_storage_vector_start:
    POWERPC_SAVE_AND_ENTER 272
powerpc_data_storage_vector_end:

.section .text.powerpc_instruction_storage_vector, "ax"
.p2align 2
.globl powerpc_instruction_storage_vector_start
.globl powerpc_instruction_storage_vector_end
powerpc_instruction_storage_vector_start:
    POWERPC_SAVE_AND_ENTER 272,248
powerpc_instruction_storage_vector_end:

.section .text.powerpc_decrementer_vector, "ax"
.p2align 2
.globl powerpc_decrementer_vector_start
.globl powerpc_decrementer_vector_end
powerpc_decrementer_vector_start:
    POWERPC_SAVE_AND_ENTER 275
powerpc_decrementer_vector_end:

.section .text.powerpc_external_vector, "ax"
.p2align 2
.globl powerpc_external_vector_start
.globl powerpc_external_vector_end
powerpc_external_vector_start:
    POWERPC_SAVE_AND_ENTER 275,248
powerpc_external_vector_end:

.section .text.powerpc_system_call_vector, "ax"
.p2align 2
.globl powerpc_system_call_vector_start
.globl powerpc_system_call_vector_end
powerpc_system_call_vector_start:
    POWERPC_SAVE_AND_ENTER 272,244
powerpc_system_call_vector_end:

.section .text, "ax"
.p2align 2
.globl powerpc_data_storage_high_handler
.type powerpc_data_storage_high_handler,@function
powerpc_data_storage_high_handler:
    POWERPC_MOVE_EXCEPTION_FRAME_TO_THREAD_STACK
    mr 3,31
    lwz 4,152(31)
    lwz 5,156(31)
    addi 1,1,-32 // C++ callee linkage area must not overlap RegisterState.
    b powerpc_data_storage_exception_handler
.size powerpc_data_storage_high_handler,.-powerpc_data_storage_high_handler

.p2align 2
.globl powerpc_instruction_storage_high_handler
.type powerpc_instruction_storage_high_handler,@function
powerpc_instruction_storage_high_handler:
    POWERPC_MOVE_EXCEPTION_FRAME_TO_THREAD_STACK
    mr 3,31
    addi 1,1,-32
    b powerpc_instruction_storage_exception_handler
.size powerpc_instruction_storage_high_handler,.-powerpc_instruction_storage_high_handler

.p2align 2
.globl powerpc_decrementer_high_handler
.type powerpc_decrementer_high_handler,@function
powerpc_decrementer_high_handler:
    POWERPC_MOVE_EXCEPTION_FRAME_TO_THREAD_STACK
    mr 3,31
    addi 1,1,-32
    b powerpc_decrementer_exception_handler
.size powerpc_decrementer_high_handler,.-powerpc_decrementer_high_handler

.p2align 2
.globl powerpc_external_high_handler
.type powerpc_external_high_handler,@function
powerpc_external_high_handler:
    POWERPC_MOVE_EXCEPTION_FRAME_TO_THREAD_STACK
    mr 3,31
    addi 1,1,-32
    b powerpc_external_exception_handler
.size powerpc_external_high_handler,.-powerpc_external_high_handler

.p2align 2
.globl powerpc_system_call_high_handler
.type powerpc_system_call_high_handler,@function
powerpc_system_call_high_handler:
    // The real-mode scratch frame cannot stay on the fixed exception stack:
    // a syscall can block or fault. Copy it onto the current thread's kernel
    // stack before entering the shared C++ syscall handler.
    mr 30,1
    stwu 1,-64(1)
    bl powerpc_current_kernel_stack_top
    mr 10,30
    addi 1,3,-320
    mr 31,1
    mr 8,1
    li 9,64
    mtctr 9
.Lcopy_syscall_frame:
    lwz 9,0(10)
    stw 9,0(8)
    addi 10,10,4
    addi 8,8,4
    bdnz .Lcopy_syscall_frame
    mr 3,31
    addi 1,1,-32
    b powerpc_syscall_exception_handler
.size powerpc_system_call_high_handler,.-powerpc_system_call_high_handler
