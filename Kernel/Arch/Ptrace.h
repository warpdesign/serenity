/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>

namespace Kernel {

class Thread;
struct RegisterState;

ErrorOr<void> enable_ptrace_single_step(Thread&, RegisterState&);

}
