/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

// Exception vectors execute with IR and DR clear. These macros only clobber
// the named GPR; callers must save and restore it with the interrupted frame.
// They do not alter CR or CTR.
.macro POWERPC_USE_KERNEL_HIGH_SEGMENTS scratch
    sync
    lis \scratch, 0x2000
    ori \scratch, \scratch, 12
    mtsr 12, \scratch
    addi \scratch, \scratch, 1
    mtsr 13, \scratch
    addi \scratch, \scratch, 1
    mtsr 14, \scratch
    addi \scratch, \scratch, 1
    mtsr 15, \scratch
    isync
.endm

// Low SR1 contains Kp=1 | (process VSID base + 1). Its 16-VSID block has
// unused high VSIDs at base + 12 through base + 15.
.macro POWERPC_USE_USER_HIGH_SEGMENTS scratch
    sync
    mfsr \scratch, 1
    addi \scratch, \scratch, 11
    mtsr 12, \scratch
    addi \scratch, \scratch, 1
    mtsr 13, \scratch
    addi \scratch, \scratch, 1
    mtsr 14, \scratch
    addi \scratch, \scratch, 1
    mtsr 15, \scratch
    isync
.endm
