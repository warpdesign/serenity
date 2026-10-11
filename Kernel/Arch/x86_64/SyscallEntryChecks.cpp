/*
 * Copyright (c) 2018-2026, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2022-2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/RegisterState.h>
#include <Kernel/Arch/SyscallEntry.h>
#include <Kernel/Arch/x86_64/ASM_wrapper.h>
#include <Kernel/Library/Panic.h>

namespace Kernel {

void prepare_syscall_entry()
{
    clac();
}

void validate_syscall_entry_registers(RegisterState const& registers)
{
    constexpr FlatPtr iopl_mask = 3u << 12;
    if ((registers.flags() & iopl_mask) != 0)
        PANIC("Syscall from process with IOPL != 0");
}

}
