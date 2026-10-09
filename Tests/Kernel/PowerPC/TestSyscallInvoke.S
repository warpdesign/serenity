/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

.section .text.syscall_invoke_vector, "ax"
.globl powerpc_syscall_invoke_vector_start
.globl powerpc_syscall_invoke_vector_end
powerpc_syscall_invoke_vector_start:
    ba powerpc_syscall_invoke_handler
powerpc_syscall_invoke_vector_end:

.section .text, "ax"
.type powerpc_syscall_invoke_handler,@function
powerpc_syscall_invoke_handler:
    mtspr 275,12
    lis 12,powerpc_syscall_observed@ha
    addi 12,12,powerpc_syscall_observed@l
    stw 0,0(12)
    stw 3,4(12)
    stw 4,8(12)
    stw 5,12(12)
    stw 6,16(12)
    lis 12,powerpc_syscall_response@ha
    lwz 3,powerpc_syscall_response@l(12)
    mfspr 12,275
    rfi
.size powerpc_syscall_invoke_handler, .-powerpc_syscall_invoke_handler
