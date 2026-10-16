/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Devices/GPU/ATI/Radeon2DBlitPlan.h>
#include <cstdlib>

extern "C" void ak_verification_failed(char const*)
{
    std::abort();
}

static bool exercise(FBCopyArea const& area)
{
    constexpr unsigned extent = 8;
    auto plan = Kernel::Radeon::plan_2d_blit(area, extent, extent);
    if (!plan.has_value())
        return false;

    unsigned expected[extent * extent];
    unsigned actual[extent * extent];
    unsigned saved[extent * extent];
    for (unsigned i = 0; i < extent * extent; ++i)
        expected[i] = actual[i] = i + 1;
    for (unsigned y = 0; y < area.height; ++y) {
        for (unsigned x = 0; x < area.width; ++x)
            saved[y * area.width + x] = expected[(area.source_y + y) * extent + area.source_x + x];
    }
    for (unsigned y = 0; y < area.height; ++y) {
        for (unsigned x = 0; x < area.width; ++x)
            expected[(area.destination_y + y) * extent + area.destination_x + x] = saved[y * area.width + x];
    }

    auto source_x = plan->source_y_x & 0xffff;
    auto source_y = plan->source_y_x >> 16;
    auto destination_x = plan->destination_y_x & 0xffff;
    auto destination_y = plan->destination_y_x >> 16;
    int x_step = (plan->direction & 1) ? 1 : -1;
    int y_step = (plan->direction & 2) ? 1 : -1;
    for (unsigned y = 0; y < area.height; ++y) {
        for (unsigned x = 0; x < area.width; ++x) {
            actual[destination_y * extent + destination_x] = actual[source_y * extent + source_x];
            source_x += x_step;
            destination_x += x_step;
        }
        source_x = plan->source_y_x & 0xffff;
        destination_x = plan->destination_y_x & 0xffff;
        source_y += y_step;
        destination_y += y_step;
    }
    for (unsigned i = 0; i < extent * extent; ++i) {
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
    if (Kernel::Radeon::plan_2d_blit({ 6, 0, 0, 0, 3, 1 }, 8, 8).has_value()
        || Kernel::Radeon::plan_2d_blit({ 0, 0, 0, 0, 0, 1 }, 8, 8).has_value()
        || Kernel::Radeon::plan_2d_blit({ 0, 0, 0, 0, 1, 1 }, 0x10000, 8).has_value())
        return 2;
    if (argc > 1) {
        for (unsigned sx = 0; sx < 8; ++sx) {
            for (unsigned sy = 0; sy < 8; ++sy) {
                for (unsigned dx = 0; dx < 8; ++dx) {
                    for (unsigned dy = 0; dy < 8; ++dy) {
                        for (unsigned width = 1; width <= 8 - (sx > dx ? sx : dx); ++width) {
                            for (unsigned height = 1; height <= 8 - (sy > dy ? sy : dy); ++height) {
                                if (!exercise({ sx, sy, dx, dy, width, height }))
                                    return 3;
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}
