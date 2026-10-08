/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

// These position-independent templates are copied to physical vector slots
// after the last Open Firmware call. Each must fit in one vector slot.
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

.section .text, "ax"
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
