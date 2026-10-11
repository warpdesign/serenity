/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

namespace Kernel {

struct RegisterState;

void prepare_syscall_entry();
void validate_syscall_entry_registers(RegisterState const&);

}
