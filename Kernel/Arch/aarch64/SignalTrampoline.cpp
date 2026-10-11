/*
 * Copyright (c) 2018-2026, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2023-2026, Timon Kruiper <timonkruiper@gmail.com>
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
    // Stack layout is assembled by Thread::dispatch_signal().
    constexpr static auto offset_to_return_value_slot = sizeof(__ucontext) + sizeof(siginfo) + sizeof(FPUState) + 4 * sizeof(FlatPtr);

    asm(
        ".global asm_signal_trampoline\n"
        "asm_signal_trampoline:\n"

        // Store x0 (return value from a syscall) into the register slot, such that we can return the correct value in sys$sigreturn.
        "str x0, [sp, %[offset_to_return_value_slot]]\n"
        // Load the handler address into x3.
        "ldr x3, [sp, #0]\n"
        // Load the signal number into the first argument.
        "ldr x0, [sp, #8]\n"
        // Load a pointer to the signal_info structure into the second argument.
        "ldr x1, [sp, #16]\n"
        // Load a pointer to the ucontext into the third argument.
        "ldr x2, [sp, #24]\n"
        // Pop the values off the stack.
        "add sp, sp, 32\n"
        // Call the signal handler.
        "blr x3\n"

        // Call sys$sigreturn.
        "mov x8, %[sigreturn_syscall_number]\n"
        "svc #0\n"
        // We should never return, so trap if we do return.
        "brk #0\n"
        "\n"
        ".global asm_signal_trampoline_end\n"
        "asm_signal_trampoline_end: \n" ::[sigreturn_syscall_number] "i"(Syscall::SC_sigreturn),
        [offset_to_return_value_slot] "i"(offset_to_return_value_slot));
}

}
