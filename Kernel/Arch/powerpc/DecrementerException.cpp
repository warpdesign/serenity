/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/Processor.h>
#include <Kernel/Arch/TrapFrame.h>
#include <Kernel/Arch/powerpc/KernelContext.h>
#include <Kernel/Arch/powerpc/RegisterState.h>
#include <Kernel/Arch/powerpc/Time/Timer.h>

namespace Kernel {

extern "C" [[noreturn]] void powerpc_decrementer_exception_handler(RegisterState*);

extern "C" [[noreturn]] void powerpc_decrementer_exception_handler(RegisterState* registers)
{
    VERIFY(registers);
    auto previous_mode = registers->previous_mode();
    TrapFrame trap { registers };
    Processor::current().enter_trap(trap, true);
    PowerPC::Timer::the().handle_interrupt();
    Processor::current().exit_trap(trap);
    if (previous_mode == ExecutionMode::User)
        powerpc_return_to_user_from_high(*registers);
    powerpc_restore_supervisor_exception_state(*registers);
}

}
