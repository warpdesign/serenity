/*
 * Copyright (c) 2020-2026, Itamar S. <itamar8910@gmail.com>
 * Copyright (c) 2020-2026, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/Ptrace.h>
#include <Kernel/Arch/aarch64/Registers.h>
#include <Kernel/Debug.h>
#include <Kernel/Tasks/Thread.h>

namespace Kernel {

ErrorOr<void> enable_ptrace_single_step(Thread& peer, RegisterState& peer_saved_registers)
{
    // Single stepping on AArch64 works by setting the SS flag in the SPSR_EL1 register.
    // When an exception return is executed in EL1, the value of SPSR_EL1.SS is copied
    // to PSTATE.SS. To enable single stepping, the MDSCR_EL1.SS must also be set to 1.
    peer_saved_registers.spsr_el1 |= Aarch64::SPSR_EL1_SS_FLAG;
    peer->debug_register_state().mdscr_el1 |= Aarch64::MDSCR_EL1_SS_FLAG;
    return {};
}

ErrorOr<FlatPtr> Thread::peek_debug_register(u32 register_index)
{
    (void)register_index;
    dbgln("FIXME: Implement Thread::peek_debug_register on AArch64");
    return ENOTSUP;
}

ErrorOr<void> Thread::poke_debug_register(u32 register_index, FlatPtr data)
{
    (void)register_index;
    (void)data;
    dbgln("FIXME: Implement Thread::poke_debug_register on AArch64");
    return ENOTSUP;
}

}
