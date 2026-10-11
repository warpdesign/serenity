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

ErrorOr<void> enable_ptrace_single_step([[maybe_unused]] Thread& peer, RegisterState& peer_saved_registers)
{
    // Single stepping works by setting the x86 TF flag bit in the eflags register.
    // This flag causes the cpu to enter single-stepping mode, which causes
    // Interrupt 1 (debug interrupt) to be emitted after every instruction.
    // To single step the program, we set the TF flag and continue the debuggee.
    constexpr u32 TRAP_FLAG = 0x100;
    peer_saved_registers.rflags |= TRAP_FLAG;
    return {};
}

ErrorOr<FlatPtr> Thread::peek_debug_register(u32 register_index)
{
    FlatPtr data;
    switch (register_index) {
    case 0:
        data = m_debug_register_state.dr0;
        break;
    case 1:
        data = m_debug_register_state.dr1;
        break;
    case 2:
        data = m_debug_register_state.dr2;
        break;
    case 3:
        data = m_debug_register_state.dr3;
        break;
    case 6:
        data = m_debug_register_state.dr6;
        break;
    case 7:
        data = m_debug_register_state.dr7;
        break;
    default:
        return EINVAL;
    }
    return data;
}

ErrorOr<void> Thread::poke_debug_register(u32 register_index, FlatPtr data)
{
    switch (register_index) {
    case 0:
        m_debug_register_state.dr0 = data;
        break;
    case 1:
        m_debug_register_state.dr1 = data;
        break;
    case 2:
        m_debug_register_state.dr2 = data;
        break;
    case 3:
        m_debug_register_state.dr3 = data;
        break;
    case 7:
        m_debug_register_state.dr7 = data;
        break;
    default:
        return EINVAL;
    }
    return {};
}

}
