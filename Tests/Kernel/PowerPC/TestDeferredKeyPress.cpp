/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Devices/TTY/DeferredKeyPress.h>

extern "C" int powerpc_deferred_key_press_test()
{
    KeyEvent event {};
    event.key = Key_A;
    event.code_point = 'a';
    event.flags = Is_Press | Mod_Shift;
    event.scancode = 0x1234'5678'9abc'def0ull;

    struct Observed {
        KeyCode key { Key_Invalid };
        u32 code_point { 0 };
        u8 flags { 0 };
    } observed;
    auto callback = Kernel::make_deferred_key_press(event, [&observed](KeyCode key, u32 code_point, u8 flags) {
        observed.key = key;
        observed.code_point = code_point;
        observed.flags = flags;
    });

    event.key = Key_B;
    event.code_point = 'b';
    event.flags = 0;
    callback();
    return observed.key == Key_A && observed.code_point == 'a'
        && observed.flags == (Is_Press | Mod_Shift);
}
