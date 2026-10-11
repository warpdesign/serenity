/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/SyscallEntry.h>

namespace Kernel {

void prepare_syscall_entry()
{
    // The exception vector has already switched to the supervisor frame.
}

void validate_syscall_entry_registers(RegisterState const&)
{
}

}
