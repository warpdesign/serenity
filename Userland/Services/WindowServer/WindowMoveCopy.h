/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>
#include <Kernel/API/Ioctl.h>
#include <string.h>

namespace WindowServer {

// Keep the software backing bitmap identical to a completed screen blit.
inline void copy_rectangle_in_place(u32* pixels, size_t pitch_in_bytes, FBCopyArea const& area)
{
    auto* bytes = reinterpret_cast<u8*>(pixels);
    size_t row_bytes = static_cast<size_t>(area.width) * sizeof(u32);
    if (area.destination_y > area.source_y) {
        for (unsigned row = area.height; row > 0; --row) {
            unsigned y = row - 1;
            memmove(bytes + (area.destination_y + y) * pitch_in_bytes + area.destination_x * sizeof(u32),
                bytes + (area.source_y + y) * pitch_in_bytes + area.source_x * sizeof(u32), row_bytes);
        }
    } else {
        for (unsigned row = 0; row < area.height; ++row) {
            memmove(bytes + (area.destination_y + row) * pitch_in_bytes + area.destination_x * sizeof(u32),
                bytes + (area.source_y + row) * pitch_in_bytes + area.source_x * sizeof(u32), row_bytes);
        }
    }
}

}
