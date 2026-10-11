/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/TrapFrame.h>
#include <Kernel/Arch/powerpc/InitialThreadContext.h>
#include <Kernel/Arch/powerpc/RegisterState.h>
#include <Kernel/Arch/powerpc/ThreadRegisters.h>

namespace Kernel {
extern "C" void exit_kernel_thread() { }
extern "C" void thread_context_first_enter();
}

alignas(16) static u8 s_initial_stack[1024];

extern "C" int powerpc_initial_thread_context_test()
{
    auto stack_top = reinterpret_cast<FlatPtr>(s_initial_stack + sizeof(s_initial_stack));

    Kernel::ThreadRegisters kernel_registers {};
    kernel_registers.set_ip(0x12345678u);
    kernel_registers.set_sp(0x87654320u);
    kernel_registers.srr1 = 0xa030u;
    kernel_registers.gpr[3] = 0x34567890u;
    kernel_registers.gpr[14] = 0x45678901u;
    Kernel::TrapFrame* kernel_trap = nullptr;
    auto kernel_context_sp = Kernel::prepare_initial_thread_context(kernel_registers, stack_top, kernel_trap);
    if (kernel_context_sp != stack_top - 64 - sizeof(Kernel::RegisterState) - sizeof(Kernel::TrapFrame)
        || kernel_trap != reinterpret_cast<Kernel::TrapFrame*>(kernel_context_sp)
        || kernel_trap->next_trap != nullptr
        || kernel_trap->regs != reinterpret_cast<Kernel::RegisterState*>(kernel_context_sp + sizeof(Kernel::TrapFrame))
        || kernel_trap->regs->ip() != 0x12345678u
        || kernel_trap->regs->userspace_sp() != stack_top - 64
        || *reinterpret_cast<FlatPtr const*>(stack_top - 64) != 0
        || kernel_trap->regs->srr1 != 0xa030u
        || kernel_trap->regs->gpr[3] != 0x34567890u
        || kernel_trap->regs->gpr[14] != 0x45678901u
        || kernel_trap->regs->lr != FlatPtr(&Kernel::exit_kernel_thread)
        || kernel_registers.sp() != kernel_context_sp
        || kernel_registers.ip() != FlatPtr(&Kernel::thread_context_first_enter))
        return 0;

    Kernel::ThreadRegisters user_registers {};
    user_registers.set_ip(0x23456780u);
    user_registers.set_sp(0x34567890u);
    user_registers.srr1 = 0xe030u;
    user_registers.gpr[2] = 0x45678910u;
    user_registers.lr = 0x56789012u;
    Kernel::TrapFrame* user_trap = nullptr;
    auto user_context_sp = Kernel::prepare_initial_thread_context(user_registers, stack_top, user_trap);
    return user_context_sp == kernel_context_sp
        && user_trap->regs->ip() == 0x23456780u
        && user_trap->regs->userspace_sp() == 0x34567890u
        && user_trap->regs->srr1 == 0xe030u
        && user_trap->regs->gpr[2] == 0x45678910u
        && user_trap->regs->lr == 0x56789012u;
}
