/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

// This position-independent template is copied to physical address 0x700
// after the last Open Firmware call. Keep it smaller than one vector slot.
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
