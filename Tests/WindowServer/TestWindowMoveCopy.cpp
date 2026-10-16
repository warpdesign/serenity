/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Userland/Services/WindowServer/WindowMoveCopy.h>

static bool exercise(FBCopyArea const& area)
{
    constexpr unsigned stride = 8;
    u32 actual[stride * stride];
    u32 expected[stride * stride];
    u32 saved[stride * stride];
    for (unsigned i = 0; i < stride * stride; ++i)
        actual[i] = expected[i] = i + 1;
    for (unsigned row = 0; row < area.height; ++row) {
        for (unsigned column = 0; column < area.width; ++column)
            saved[row * area.width + column] = expected[(area.source_y + row) * stride + area.source_x + column];
    }
    for (unsigned row = 0; row < area.height; ++row) {
        for (unsigned column = 0; column < area.width; ++column)
            expected[(area.destination_y + row) * stride + area.destination_x + column] = saved[row * area.width + column];
    }
    WindowServer::copy_rectangle_in_place(actual, stride * sizeof(u32), area);
    for (unsigned i = 0; i < stride * stride; ++i) {
        if (actual[i] != expected[i])
            return false;
    }
    return true;
}

int main(int argc, char**)
{
    if (!exercise({ 1, 1, 2, 2, 4, 4 }) || !exercise({ 2, 2, 1, 1, 4, 4 })
        || !exercise({ 1, 1, 2, 1, 4, 2 }) || !exercise({ 1, 1, 1, 2, 2, 4 }))
        return 1;
    if (argc > 1) {
        for (unsigned sx = 0; sx < 8; ++sx) {
            for (unsigned sy = 0; sy < 8; ++sy) {
                for (unsigned dx = 0; dx < 8; ++dx) {
                    for (unsigned dy = 0; dy < 8; ++dy) {
                        for (unsigned width = 1; width <= 8 - (sx > dx ? sx : dx); ++width) {
                            for (unsigned height = 1; height <= 8 - (sy > dy ? sy : dy); ++height) {
                                if (!exercise({ sx, sy, dx, dy, width, height }))
                                    return 2;
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}
