/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/Processor.h>
#include <Kernel/Arch/TrapFrame.h>
#include <Kernel/Arch/powerpc/KernelContext.h>
#include <Kernel/Arch/powerpc/RegisterState.h>
#include <Kernel/Tasks/Thread.h>

namespace Kernel {

extern "C" void syscall_handler(TrapFrame*);

extern "C" FlatPtr powerpc_current_kernel_stack_top();
extern "C" FlatPtr powerpc_current_kernel_stack_top()
{
    auto* thread = Thread::current();
    VERIFY(thread);
    return thread->kernel_stack_top();
}

extern "C" [[noreturn]] void powerpc_syscall_exception_handler(RegisterState*);
extern "C" [[noreturn]] void powerpc_syscall_exception_handler(RegisterState* registers)
{
    VERIFY(registers);
    VERIFY(registers->previous_mode() == ExecutionMode::User);

    TrapFrame trap { registers };
    Processor::current().enter_trap(trap, false);
    Processor::enable_interrupts();
    syscall_handler(&trap);
    Processor::disable_interrupts();
    Processor::current().exit_trap(trap);
    powerpc_return_to_user_from_high(*registers);
}

}
