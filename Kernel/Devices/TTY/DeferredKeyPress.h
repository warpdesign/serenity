/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Function.h>
#include <AK/StdLibExtras.h>
#include <Kernel/API/KeyCode.h>

namespace Kernel {

template<typename Receiver>
Function<void()> make_deferred_key_press(KeyEvent const& event, Receiver&& receiver)
{
    return [key = event.key, code_point = event.code_point, flags = event.flags, receiver = forward<Receiver>(receiver)]() mutable {
        receiver(key, code_point, flags);
    };
}

}
