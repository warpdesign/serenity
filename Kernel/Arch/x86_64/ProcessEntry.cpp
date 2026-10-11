/*
 * Copyright (c) 2018-2026, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2022-2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/CPUID.h>
#include <Kernel/Arch/ProcessEntry.h>
#include <Kernel/Arch/ThreadRegisters.h>

namespace Kernel {

void set_process_entry_arguments(ThreadRegisters& registers, size_t argc, FlatPtr argv, FlatPtr envp)
{
    registers.rdi = argc;
    registers.rsi = argv;
    registers.rdx = envp;
}

long process_entry_hwcap()
{
    return static_cast<long>(CPUID(1).edx());
}

}
