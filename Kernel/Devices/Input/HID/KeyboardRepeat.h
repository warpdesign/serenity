/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Optional.h>
#include <AK/Types.h>
#include <Kernel/API/KeyCode.h>

namespace Kernel::HID {

constexpr bool key_may_repeat(u16 usage_id, KeyCode key)
{
    if (usage_id >= 0xe0 && usage_id <= 0xe7)
        return false;
    return key != Key_Invalid && key != Key_CapsLock && key != Key_NumLock && key != Key_ScrollLock;
}

// USB HID reports held-key state, not typematic key-down events.
class KeyboardRepeat {
public:
    static constexpr u32 initial_delay_ms = 500;
    static constexpr u32 interval_ms = 40;

    u32 press(u16 usage_id)
    {
        m_active_usage = usage_id;
        return ++m_generation;
    }

    void release(u16 usage_id)
    {
        if (m_active_usage == usage_id) {
            m_active_usage = 0;
            ++m_generation;
        }
    }

    void stop()
    {
        m_active_usage = 0;
        ++m_generation;
    }

    Optional<u16> on_timer(u32 generation) const
    {
        if (generation != m_generation || m_active_usage == 0)
            return {};
        return m_active_usage;
    }

private:
    u16 m_active_usage { 0 };
    u32 m_generation { 0 };
};

}
