/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Devices/Input/HID/KeyboardRepeat.h>
#include <cstdlib>

extern "C" void ak_verification_failed(char const*)
{
    std::abort();
}

using Kernel::HID::KeyboardRepeat;

static_assert(KeyboardRepeat::initial_delay_ms > KeyboardRepeat::interval_ms);
static_assert(KeyboardRepeat::initial_delay_ms == 500);
static_assert(KeyboardRepeat::interval_ms == 40);
static_assert(Kernel::HID::key_may_repeat(0x04, Key_A));
static_assert(Kernel::HID::key_may_repeat(0x2a, Key_Backspace));
static_assert(!Kernel::HID::key_may_repeat(0xe1, Key_LeftShift));
static_assert(!Kernel::HID::key_may_repeat(0x39, Key_CapsLock));
static_assert(!Kernel::HID::key_may_repeat(0x53, Key_NumLock));

int main(int argc, char**)
{
    KeyboardRepeat keyboard;
    auto first_press = keyboard.press(0x04); // A
    // A HID report contains one press transition even if the key is held.
    unsigned delivered_presses = 1;
    for (unsigned elapsed_ms = 0; elapsed_ms < 620; elapsed_ms += KeyboardRepeat::interval_ms) {
        if (elapsed_ms < KeyboardRepeat::initial_delay_ms)
            continue;
        if (keyboard.on_timer(first_press) == 0x04)
            ++delivered_presses;
    }
    if (delivered_presses != 4)
        return 1;

    keyboard.release(0x04);
    if (keyboard.on_timer(first_press).has_value())
        return 1;

    if (argc > 1) {
        auto second_press = keyboard.press(0x2a); // Backspace
        if (keyboard.on_timer(first_press).has_value() || keyboard.on_timer(second_press) != 0x2a)
            return 1;
        keyboard.release(0x04); // Releasing an older key must not stop Backspace.
        if (keyboard.on_timer(second_press) != 0x2a)
            return 1;
        keyboard.stop(); // USB disconnect.
        if (keyboard.on_timer(second_press).has_value())
            return 1;
    }
    return 0;
}
