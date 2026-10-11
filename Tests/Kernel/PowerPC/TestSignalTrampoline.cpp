/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/API/Syscall.h>
#include <Kernel/API/POSIX/signal.h>
#include <Kernel/API/POSIX/ucontext.h>
#include <Kernel/Arch/FPUState.h>

extern "C" char const asm_signal_trampoline[];
extern "C" char const asm_signal_trampoline_end[];

extern "C" int powerpc_signal_trampoline_test()
{
    auto byte_count = asm_signal_trampoline_end - asm_signal_trampoline;
    if (byte_count != 13 * sizeof(u32) || byte_count >= 4096)
        return 0;

    auto const* instructions = reinterpret_cast<u32 const*>(asm_signal_trampoline);
    constexpr u32 return_value_offset = sizeof(__ucontext) + sizeof(siginfo) + sizeof(Kernel::FPUState) + 4 * sizeof(FlatPtr);
    if (return_value_offset > 0xffffu)
        return 0;
    return instructions[0] == (0x90610000u | return_value_offset)
        && instructions[6] == 0x9421fff0u
        && instructions[8] == 0x4e800421u
        && instructions[10] == (0x38000000u | Syscall::SC_sigreturn)
        && instructions[11] == 0x44000002u
        && instructions[12] == 0x7fe00008u;
}
