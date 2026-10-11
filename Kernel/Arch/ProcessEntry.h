/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>

namespace Kernel {

struct ThreadRegisters;

void set_process_entry_arguments(ThreadRegisters&, size_t argc, FlatPtr argv, FlatPtr envp);
long process_entry_hwcap();

}
