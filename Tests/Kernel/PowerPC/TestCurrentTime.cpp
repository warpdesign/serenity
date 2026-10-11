/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/CurrentTime.h>

extern "C" int powerpc_current_time_fallback_test()
{
    return Kernel::optional_current_time() == nullptr;
}
