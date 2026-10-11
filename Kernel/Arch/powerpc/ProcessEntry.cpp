/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/ProcessEntry.h>
#include <Kernel/Arch/ThreadRegisters.h>

namespace Kernel {

void set_process_entry_arguments(ThreadRegisters& registers, size_t argc, FlatPtr argv, FlatPtr envp)
{
    registers.gpr[3] = argc;
    registers.gpr[4] = argv;
    registers.gpr[5] = envp;
}

long process_entry_hwcap()
{
    // No SerenityOS PowerPC AT_HWCAP bit definitions exist yet.
    return 0;
}

}
