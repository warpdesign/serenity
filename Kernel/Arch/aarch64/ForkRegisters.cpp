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
    child_regs.x[0] = 0; // fork() returns 0 in the child :^)
    for (size_t i = 1; i < array_size(child_regs.x); ++i)
        child_regs.x[i] = regs.x[i];
    child_regs.spsr_el1 = regs.spsr_el1;
    child_regs.elr_el1 = regs.elr_el1;
    child_regs.sp_el0 = regs.sp_el0;
    child_regs.tpidr_el0 = regs.tpidr_el0;
}

}
