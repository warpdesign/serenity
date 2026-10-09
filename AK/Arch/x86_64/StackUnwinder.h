/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

namespace AK {

struct StackFrameLayout {
    static constexpr ptrdiff_t previous_frame_pointer_offset = 0;
    static constexpr ptrdiff_t return_address_offset = 8;
    static constexpr bool return_address_in_previous_frame = false;
};

}
