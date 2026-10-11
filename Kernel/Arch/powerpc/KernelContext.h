/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>

namespace Kernel {
class Thread;
struct RegisterState;
struct ThreadRegisters;
extern "C" void powerpc_enter_thread_context(Thread* from_thread, Thread* to_thread);
extern "C" [[noreturn]] void powerpc_restore_supervisor_exception_state(RegisterState const&);
// The frame reference must be a physical address with IR and DR disabled.
extern "C" [[noreturn]] void powerpc_restore_user_exception_state_real_mode(RegisterState const&);
// The frame reference is a high kernel virtual address; IR and DR are off.
extern "C" [[noreturn]] void powerpc_restore_user_exception_state_from_high_real_mode(RegisterState const&);
// Enter the low real-mode return trampoline atomically from translated kernel code.
extern "C" [[noreturn]] void powerpc_return_to_user_from_high(RegisterState const&);
extern "C" [[noreturn]] void powerpc_start_initial_context(ThreadRegisters const&, Thread*);
extern "C" [[noreturn]] void powerpc_start_assumed_context(FlatPtr prepared_stack_pointer, Thread*);
extern "C" void do_assume_context(Thread*, u32 interrupt_state);
extern "C" FlatPtr powerpc_prepare_assume_context(Thread*, u32 interrupt_state);
}

// Save the outgoing kernel call context and jump to the incoming one. The
// returned pointer names the thread that switched into the resumed context.
extern "C" Kernel::Thread* powerpc_switch_kernel_context(Kernel::ThreadRegisters& outgoing,
    Kernel::ThreadRegisters const& incoming, Kernel::Thread* from_thread, Kernel::Thread* to_thread);
