/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Library/Assertions.h>

[[noreturn]] void __assertion_failed(char const*, char const*, unsigned, char const*)
{
    for (;;) { }
}
