/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "HighSegmentSwitch.inc"
#include "KernelContextOffsets.h"

// Copied to the low 0x300 exception slot after Open Firmware is finished.
// SPRG0 contains the physical top of the dedicated kernel exception stack.
.section .text.powerpc_data_storage_vector, "ax"
.p2align 2
.globl powerpc_data_storage_vector_start
.globl powerpc_data_storage_vector_end
powerpc_data_storage_vector_start:
    mtspr 273,0
    mtspr 274,1
    mfspr 1,272
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
    lwz 12,252(1)
    mtspr 26,12
    mfmsr 12
    ori 12,12,0x32 // IR | DR | recoverable, with interrupts disabled.
    mtspr 27,12
    rfi
powerpc_data_storage_vector_end:

.section .text, "ax"
.p2align 2
.globl powerpc_data_storage_high_handler
.type powerpc_data_storage_high_handler,@function
powerpc_data_storage_high_handler:
    mr 3,1
    lwz 4,152(1)
    lwz 5,156(1)
    addi 1,1,-32 // C++ callee linkage area must not overlap RegisterState.
    b powerpc_data_storage_exception_handler
.size powerpc_data_storage_high_handler,.-powerpc_data_storage_high_handler
