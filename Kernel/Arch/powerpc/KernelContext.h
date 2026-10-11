/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

namespace Kernel {
class Thread;
struct RegisterState;
struct ThreadRegisters;
extern "C" void powerpc_enter_thread_context(Thread* from_thread, Thread* to_thread);
extern "C" [[noreturn]] void powerpc_restore_supervisor_exception_state(RegisterState const&);
extern "C" [[noreturn]] void powerpc_start_initial_context(ThreadRegisters const&, Thread*);
}

// Save the outgoing kernel call context and jump to the incoming one. The
// returned pointer names the thread that switched into the resumed context.
extern "C" Kernel::Thread* powerpc_switch_kernel_context(Kernel::ThreadRegisters& outgoing,
    Kernel::ThreadRegisters const& incoming, Kernel::Thread* from_thread, Kernel::Thread* to_thread);
