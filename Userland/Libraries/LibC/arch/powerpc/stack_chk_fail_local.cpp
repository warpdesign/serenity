/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

extern "C" [[noreturn]] void __stack_chk_fail();
extern "C" [[noreturn, gnu::visibility("hidden"), gnu::no_stack_protector]] void __stack_chk_fail_local();

extern "C" [[noreturn, gnu::visibility("hidden"), gnu::no_stack_protector]] void __stack_chk_fail_local()
{
    __stack_chk_fail();
}
