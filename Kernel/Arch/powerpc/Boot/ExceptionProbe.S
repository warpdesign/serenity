/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "ProbeAddresses.h"

// These templates are copied to physical vector slots after the last Open
// Firmware call. Each must fit in one slot and retain a valid branch target.
.section .text.data_storage_exception_probe, "ax"
.global data_storage_exception_vector_start
.global data_storage_exception_vector_end
data_storage_exception_vector_start:
    mtspr 275, 11     // SPRG3: preserve r11 from the interrupted code.
    mtspr 273, 12     // Temporarily preserve r12 while recording the kernel SR.
    mfspr 11, 272     // SPRG0: count data-storage exceptions.
    addi 11, 11, 1
    mtspr 272, 11
    mfspr 11, 19      // DAR: faulting effective address.
    mtspr 274, 11     // SPRG2

    // The vector runs in real mode. Restore shared kernel high VSIDs before
    // any translated kernel access in a future full handler.
    sync
    lis 11, 0x2000
    ori 11, 11, 12
    mtsr 12, 11
    addi 11, 11, 1
    mtsr 13, 11
    addi 11, 11, 1
    mtsr 14, 11
    addi 11, 11, 1
    mtsr 15, 11
    isync
    mfsr 11, 12
    lis 12, powerpc_probe_dsi_high_segment@ha
    stw 11, powerpc_probe_dsi_high_segment@l(12)
    mfspr 12, 273     // Restore the interrupted r12.

    mfspr 11, 26      // Skip the probe's faulting load.
    addi 11, 11, 4
    mtspr 26, 11

    mfcr 11
    mtspr 273, 11     // Use SPRG1 to preserve CR across the mode test.
    mfspr 11, 27
    andi. 11, 11, 0x4000
    beq .Lrestore_data_storage_state

    // Low SR1 holds the process VSID base + 1. Restore its unused high VSIDs
    // before rfi to keep the read-only kernel alias absent in user mode.
    sync
    mfsr 11, 1
    addi 11, 11, 11
    mtsr 12, 11
    addi 11, 11, 1
    mtsr 13, 11
    addi 11, 11, 1
    mtsr 14, 11
    addi 11, 11, 1
    mtsr 15, 11
    isync

.Lrestore_data_storage_state:
    mfspr 11, 273
    mtcrf 255, 11
    mfspr 11, 18      // DSISR: data-storage fault cause.
    mtspr 273, 11     // SPRG1, replacing the temporary CR value.
    mfspr 11, 275
    rfi
data_storage_exception_vector_end:

.section .text.external_interrupt_probe, "ax"
.global external_interrupt_vector_start
.global external_interrupt_vector_end
external_interrupt_vector_start:
    ba powerpc_external_interrupt_probe_handler
external_interrupt_vector_end:

.section .text.exception_probe, "ax"
.global program_exception_vector_start
.global program_exception_vector_end
program_exception_vector_start:
    mtspr 275, 11     // SPRG3: preserve r11 from the interrupted code.
    mfspr 11, 273     // SPRG1: count the exception.
    addi 11, 11, 1
    mtspr 273, 11
    mfspr 11, 27      // Save SRR1 for the trap-cause check.
    mtspr 274, 11     // SPRG2
    mfspr 11, 26      // SRR0 points at the trapping instruction.
    addi 11, 11, 4
    mtspr 26, 11
    mfspr 11, 275
    rfi
program_exception_vector_end:

.section .text.decrementer_exception_probe, "ax"
.global decrementer_exception_vector_start
.global decrementer_exception_vector_end
decrementer_exception_vector_start:
    mtspr 275, 11     // SPRG3: preserve r11.
    mfspr 11, 272     // SPRG0: interrupt count.
    addi 11, 11, 1
    mtspr 272, 11
    mfspr 11, 274     // SPRG2: probe period.
    mtdec 11
    mfspr 11, 275
    rfi
decrementer_exception_vector_end:

.section .text, "ax"
.type powerpc_external_interrupt_probe_handler, @function
powerpc_external_interrupt_probe_handler:
    // Hardware enters with IR/DR clear. The interrupted r1 may be a high
    // virtual address, so switch to a physical stack before the first store.
    mtspr 275, 11
    mtspr 274, 12
    lis 11, powerpc_probe_exception_stack_top@ha
    addi 11, 11, powerpc_probe_exception_stack_top@l
    addi 11, 11, -224
    stw 0, 48(11)
    stw 1, 52(11)
    stmw 2, 56(11)
    mfspr 0, 275
    stw 0, 92(11)     // Original r11.
    mfspr 0, 274
    stw 0, 96(11)     // Original r12.
    li 0, 0
    stw 0, 0(11)      // End the backchain at the exception stack.
    mr 1, 11
    mfcr 0
    stw 0, 176(1)
    mfctr 0
    stw 0, 180(1)
    mfxer 0
    stw 0, 184(1)
    mflr 0
    stw 0, 188(1)
    mfspr 0, 26
    stw 0, 192(1)
    mfspr 0, 27
    stw 0, 196(1)

    lis 12, powerpc_probe_use_high_irq_handler@ha
    lwz 12, powerpc_probe_use_high_irq_handler@l(12)
    cmpwi 12, 0
    bne .Lenter_high_external_handler

    addi 3, 1, 48
    bl powerpc_external_irq_probe_dispatch
    b .Lrestore_external_state

.Lenter_high_external_handler:
    lis 1, POWERPC_HPT_EXCEPTION_STACK_SEGMENT
    ori 1, 1, 0x0f20     // Top of one page minus the 224-byte frame.
    mfmsr 12
    ori 12, 12, 0x32    // Supervisor IR | DR | RI, with EE disabled.
    mtspr 27, 12
    lis 12, powerpc_external_interrupt_high_handler@ha
    addi 12, 12, powerpc_external_interrupt_high_handler@l
    addis 12, 12, 0xc000
    mtspr 26, 12
    rfi

.global powerpc_external_interrupt_high_handler
powerpc_external_interrupt_high_handler:
    addi 3, 1, 48
    bl powerpc_external_irq_probe_dispatch

.Lrestore_external_state:
    lwz 0, 176(1)
    mtcrf 255, 0
    lwz 0, 180(1)
    mtctr 0
    lwz 0, 184(1)
    mtxer 0
    lwz 0, 188(1)
    mtlr 0
    lwz 0, 192(1)
    mtspr 26, 0
    lwz 0, 196(1)
    mtspr 27, 0
    lmw 2, 56(1)
    lwz 0, 48(1)
    lwz 1, 52(1)
    rfi
.size powerpc_external_interrupt_probe_handler, .-powerpc_external_interrupt_probe_handler

.global powerpc_run_high_stack_external_irq_probe
.type powerpc_run_high_stack_external_irq_probe, @function
powerpc_run_high_stack_external_irq_probe:
    stwu 1, -64(1)
    mflr 0
    stw 0, 68(1)
    stw 30, 56(1)
    stw 31, 60(1)
    mr 30, 3
    mr 31, 1
    bl powerpc_install_boot_bats
    bl powerpc_enable_boot_translation
    lis 1, powerpc_probe_high_stack_top@ha
    addi 1, 1, powerpc_probe_high_stack_top@l
    addis 1, 1, 0xc000
    addi 1, 1, -16
    li 0, 0
    stw 0, 0(1)
    mr 3, 30
    bl powerpc_external_irq_high_stack_body
    mr 30, 3
    mr 1, 31
    bl powerpc_disable_boot_translation
    mr 3, 30
    lwz 30, 56(1)
    lwz 31, 60(1)
    lwz 0, 68(1)
    mtlr 0
    addi 1, 1, 64
    blr
.size powerpc_run_high_stack_external_irq_probe, .-powerpc_run_high_stack_external_irq_probe

.global powerpc_run_hpt_stack_external_irq_probe
.type powerpc_run_hpt_stack_external_irq_probe, @function
powerpc_run_hpt_stack_external_irq_probe:
    stwu 1, -64(1)
    mflr 0
    stw 0, 68(1)
    stw 30, 56(1)
    stw 31, 60(1)
    mr 30, 3
    mr 31, 1
    bl powerpc_enable_boot_translation
    lis 1, POWERPC_HPT_IRQ_STACK_SEGMENT // HPT stack, no high BAT.
    ori 1, 1, 0x0ff0
    li 0, 0
    stw 0, 0(1)
    mr 3, 30
    bl powerpc_external_irq_high_stack_body
    mr 30, 3
    mr 1, 31
    bl powerpc_disable_boot_translation
    mr 3, 30
    lwz 30, 56(1)
    lwz 31, 60(1)
    lwz 0, 68(1)
    mtlr 0
    addi 1, 1, 64
    blr
.size powerpc_run_hpt_stack_external_irq_probe, .-powerpc_run_hpt_stack_external_irq_probe

.global trigger_program_exception
.type trigger_program_exception, @function
trigger_program_exception:
    tw 31, 0, 0
    blr
.size trigger_program_exception, .-trigger_program_exception

.global powerpc_probe_absent_high_alias
.type powerpc_probe_absent_high_alias, @function
powerpc_probe_absent_high_alias:
    mr 4, 3
    li 3, 0
    lwz 3, 0(4)       // The data-storage handler skips this exact instruction.
    blr
.size powerpc_probe_absent_high_alias, .-powerpc_probe_absent_high_alias

.global powerpc_probe_read_only_store
.type powerpc_probe_read_only_store, @function
powerpc_probe_read_only_store:
    stw 4, 0(3)       // The data-storage handler skips this exact instruction.
    blr
.size powerpc_probe_read_only_store, .-powerpc_probe_read_only_store

.section .bss, "aw", @nobits
.balign 4096
.global powerpc_probe_exception_stack
powerpc_probe_exception_stack:
    .space 4096
.global powerpc_probe_exception_stack_top
powerpc_probe_exception_stack_top:
.balign 4096
.global powerpc_probe_high_stack
powerpc_probe_high_stack:
    .space 4096
.global powerpc_probe_high_stack_top
powerpc_probe_high_stack_top:
.balign 4
.global powerpc_probe_use_high_irq_handler
powerpc_probe_use_high_irq_handler:
    .long 0
