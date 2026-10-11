/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Assertions.h>
#include <Kernel/Arch/TrapFrame.h>
#include <Kernel/Arch/powerpc/InitialThreadContext.h>
#include <Kernel/Arch/powerpc/RegisterState.h>
#include <Kernel/Arch/powerpc/ThreadRegisters.h>

namespace Kernel {

extern "C" void exit_kernel_thread();
extern "C" void thread_context_first_enter();

FlatPtr prepare_initial_thread_context(ThreadRegisters& registers, FlatPtr kernel_stack_top, TrapFrame*& current_trap)
{
    VERIFY((kernel_stack_top & 15) == 0);

    // An entry function may save LR in its caller's linkage area at SP + 4.
    // Keep that space above the saved exception frame even for a first entry.
    constexpr FlatPtr caller_frame_size = 64;
    auto caller_sp = kernel_stack_top - caller_frame_size;
    *reinterpret_cast<FlatPtr*>(caller_sp) = 0;
    auto stack_top = caller_sp - sizeof(RegisterState);
    auto& frame = *reinterpret_cast<RegisterState*>(stack_top);
    static_cast<PowerPCExceptionState&>(frame) = static_cast<PowerPCExceptionState const&>(registers);
    if ((registers.srr1 & 0x4000u) == 0)
        frame.gpr[1] = caller_sp;
    if (frame.lr == 0)
        frame.lr = FlatPtr(&exit_kernel_thread);

    stack_top -= sizeof(TrapFrame);
    auto& trap = *reinterpret_cast<TrapFrame*>(stack_top);
    trap.next_trap = nullptr;
    trap.regs = &frame;
    current_trap = &trap;

    registers.set_sp(stack_top);
    registers.set_ip(FlatPtr(&thread_context_first_enter));
    return stack_top;
}

}
