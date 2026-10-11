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

        // Store a0 (return value from a syscall) into the register slot, such that we can return the correct value in sys$sigreturn.
        "lui t0, %%hi(%[offset_to_return_value_slot])\n" // FIXME: Move the return value slot before the FPUState to avoid having an extra lui+add pair here.
        "add t0, t0, sp\n"
        "sd a0, %%lo(%[offset_to_return_value_slot])(t0)\n"
        // Load the handler address into t0.
        "ld t0, 0(sp)\n"
        // Load the signal number into the first argument.
        "ld a0, 8(sp)\n"
        // Load a pointer to the signal_info structure into the second argument.
        "ld a1, 16(sp)\n"
        // Load a pointer to the ucontext into the third argument.
        "ld a2, 24(sp)\n"
        // Pop the values off the stack.
        "addi sp, sp, 32\n"
        // Call the signal handler.
        "jalr t0\n"

        // Call sys$sigreturn.
        "li a7, %[sigreturn_syscall_number]\n"
        "ecall\n"
        // We should never return, so trap if we do return.
        "unimp\n"

        "\n"
        ".global asm_signal_trampoline_end\n"
        "asm_signal_trampoline_end: \n" ::[sigreturn_syscall_number] "i"(Syscall::SC_sigreturn),
        [offset_to_return_value_slot] "i"(offset_to_return_value_slot));
}

}
