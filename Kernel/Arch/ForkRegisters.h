/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

namespace Kernel {

struct RegisterState;
struct ThreadRegisters;

void initialize_fork_registers(ThreadRegisters& child, RegisterState const& parent);

}
