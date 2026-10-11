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
        ".intel_syntax noprefix\n"
        ".globl asm_signal_trampoline\n"
        "asm_signal_trampoline:\n"

        // we have to save rax 'cause it might be the return value from a syscall
        "mov [rsp+%P1], rax\n"
        // Pop the handler into rcx
        "pop rcx\n" // save handler
        // pop signal number into rdi (first param)
        "pop rdi\n"
        // pop siginfo* into rsi (second param)
        "pop rsi\n"
        // pop ucontext* into rdx (third param)
        "pop rdx\n"
        // Note that the stack is currently aligned to 16 bytes as we popped the extra entries above.
        // call the signal handler
        "call rcx\n"
        // Current stack state is just saved_rax, ucontext, signal_info, fpu_state.
        // syscall SC_sigreturn
        "mov rax, %P0\n"
        "syscall\n"
        ".globl asm_signal_trampoline_end\n"
        "asm_signal_trampoline_end:\n"
        ".att_syntax"
        :
        : "i"(Syscall::SC_sigreturn),
        "i"(offset_to_return_value_slot));
}

}
