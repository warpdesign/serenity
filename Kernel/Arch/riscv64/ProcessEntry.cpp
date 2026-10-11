/*
 * Copyright (c) 2018-2026, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2022-2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/ProcessEntry.h>
#include <Kernel/Arch/ThreadRegisters.h>

namespace Kernel {

void set_process_entry_arguments(ThreadRegisters& registers, size_t argc, FlatPtr argv, FlatPtr envp)
{
    registers.x[9] = argc;
    registers.x[10] = argv;
    registers.x[11] = envp;
}

long process_entry_hwcap()
{
    return 0;
}

}
