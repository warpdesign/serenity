/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/API/Syscall.h>
#include <Kernel/API/POSIX/signal.h>
#include <Kernel/API/POSIX/ucontext.h>
#include <Kernel/Arch/FPUState.h>

namespace Kernel {

extern void signal_trampoline_dummy() __attribute__((used));
void signal_trampoline_dummy()
{
    // Thread::dispatch_signal() puts the handler and three arguments at the
    // top of the user stack, followed by FPU state, siginfo, ucontext, and
    // the saved syscall return value. r2 remains the thread pointer.
    constexpr static auto return_value_offset = sizeof(__ucontext) + sizeof(siginfo) + sizeof(FPUState) + 4 * sizeof(FlatPtr);

    asm(
        ".global asm_signal_trampoline\n"
        "asm_signal_trampoline:\n"
        "stw 3,%[return_value_offset](1)\n"
        "lwz 12,0(1)\n"
        "lwz 3,4(1)\n"
        "lwz 4,8(1)\n"
        "lwz 5,12(1)\n"
        "addi 1,1,16\n"
        // Give the handler a standard 16-byte linkage frame with a backchain.
        "stwu 1,-16(1)\n"
        "mtctr 12\n"
        "bctrl\n"
        "lwz 1,0(1)\n"
        "li 0,%[sigreturn_syscall_number]\n"
        "sc\n"
        "tw 31,0,0\n"
        ".global asm_signal_trampoline_end\n"
        "asm_signal_trampoline_end:\n"
        :
        : [return_value_offset] "i"(return_value_offset),
          [sigreturn_syscall_number] "i"(Syscall::SC_sigreturn));
}

}
