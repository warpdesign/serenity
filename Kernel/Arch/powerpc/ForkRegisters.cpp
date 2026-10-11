/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/ForkRegisters.h>
#include <Kernel/Arch/RegisterState.h>
#include <Kernel/Arch/ThreadRegisters.h>

namespace Kernel {

void initialize_fork_registers(ThreadRegisters& child, RegisterState const& parent)
{
    // The child has its own page directory and kernel stack from Thread setup.
    // Copy only the interrupted user frame, then set fork's child return value.
    static_cast<PowerPCExceptionState&>(child) = static_cast<PowerPCExceptionState const&>(parent);
    child.gpr[3] = 0;
}

}
