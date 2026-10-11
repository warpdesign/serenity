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

void operator delete(void*) noexcept
{
    // The boot probe has no heap. Reaching outline Function storage is a bug.
    __builtin_trap();
}

void operator delete(void* pointer, __SIZE_TYPE__) noexcept
{
    operator delete(pointer);
}
