/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/Ptrace.h>
#include <Kernel/Tasks/Thread.h>

namespace Kernel {

ErrorOr<void> enable_ptrace_single_step(Thread&, RegisterState&)
{
    // The PowerPC debug exception and per-thread step state are not wired up yet.
    return ENOTSUP;
}

ErrorOr<FlatPtr> Thread::peek_debug_register(u32)
{
    return ENOTSUP;
}

ErrorOr<void> Thread::poke_debug_register(u32, FlatPtr)
{
    return ENOTSUP;
}

}
