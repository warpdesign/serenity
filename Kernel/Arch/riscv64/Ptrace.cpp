/*
 * Copyright (c) 2020-2026, Itamar S. <itamar8910@gmail.com>
 * Copyright (c) 2020-2026, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/Ptrace.h>
#include <Kernel/Debug.h>
#include <Kernel/Tasks/Thread.h>

namespace Kernel {

ErrorOr<void> enable_ptrace_single_step([[maybe_unused]] Thread& peer, [[maybe_unused]] RegisterState& peer_saved_registers)
{
    TODO_RISCV64();
}

ErrorOr<FlatPtr> Thread::peek_debug_register(u32 register_index)
{
    (void)register_index;
    dbgln("FIXME: Implement Thread::peek_debug_register on RISC-V");
    return ENOTSUP;
}

ErrorOr<void> Thread::poke_debug_register(u32 register_index, FlatPtr data)
{
    (void)register_index;
    (void)data;
    dbgln("FIXME: Implement Thread::poke_debug_register on RISC-V");
    return ENOTSUP;
}

}
