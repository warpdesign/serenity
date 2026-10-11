/*
 * Copyright (c) 2018-2026, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2022-2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/SyscallEntry.h>

namespace Kernel {

void prepare_syscall_entry()
{
    // SCTLR_EL1.SPAN == 0 restores PAN on syscall entry when FEAT_PAN exists.
}

void validate_syscall_entry_registers(RegisterState const&)
{
}

}
