/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

// These templates are copied to physical vector slots after the last Open
// Firmware call. Each must fit in one slot and retain a valid branch target.
.section .text.data_storage_exception_probe, "ax"
.global data_storage_exception_vector_start
.global data_storage_exception_vector_end
data_storage_exception_vector_start:
    mtspr 275, 11     // SPRG3: preserve r11 from the interrupted code.
    mfspr 11, 272     // SPRG0: count data-storage exceptions.
    addi 11, 11, 1
    mtspr 272, 11
    mfspr 11, 19      // DAR: faulting effective address.
    mtspr 274, 11     // SPRG2
    mfspr 11, 18      // DSISR: data-storage fault cause.
    mtspr 273, 11     // SPRG1, replacing the earlier trap count.
    mfspr 11, 26      // Skip the probe's faulting load.
    addi 11, 11, 4
    mtspr 26, 11
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
    // Supervisor-only real-mode probe. Keep the ABI linkage and parameter
    // area separate from the saved PowerPCExceptionState at r1 + 48.
    stwu 1, -224(1)
    stw 0, 48(1)
    stmw 2, 56(1)
    addi 0, 1, 224
    stw 0, 52(1)
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

    addi 3, 1, 48
    bl powerpc_external_irq_probe_dispatch

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
