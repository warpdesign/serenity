/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>

namespace Kernel {

struct ThreadRegisters;
struct TrapFrame;

// Copy the requested entry state to a trap frame on the kernel stack, then
// point the thread's scheduler context at its first-entry shim.
FlatPtr prepare_initial_thread_context(ThreadRegisters&, FlatPtr kernel_stack_top, TrapFrame*& current_trap);

}
