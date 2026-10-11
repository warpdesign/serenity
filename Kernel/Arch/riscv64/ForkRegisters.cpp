/*
 * Copyright (c) 2018-2026, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2023-2026, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/ForkRegisters.h>
#include <Kernel/Arch/RegisterState.h>
#include <Kernel/Arch/ThreadRegisters.h>
#include <Kernel/Debug.h>

namespace Kernel {

void initialize_fork_registers(ThreadRegisters& child_regs, RegisterState const& regs)
{
    for (size_t i = 0; i < array_size(child_regs.x); ++i)
        child_regs.x[i] = regs.x[i];
    child_regs.x[9] = 0; // fork() returns 0 in the child :^)
    child_regs.sstatus = regs.sstatus;
    child_regs.pc = regs.sepc;
    dbgln_if(FORK_DEBUG, "fork: child will begin executing at {:p} with stack {:p}, kstack {:p}",
        child_regs.pc, child_regs.sp(), child_regs.kernel_sp);
}

}
