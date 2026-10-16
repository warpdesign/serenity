/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Optional.h>
#include <AK/Types.h>
#include <Kernel/API/Ioctl.h>

namespace Kernel::Radeon {

struct BlitPlan {
    u32 direction;
    u32 source_y_x;
    u32 destination_y_x;
    u32 height_width;
};

constexpr Optional<BlitPlan> plan_2d_blit(FBCopyArea const& area, u32 surface_width, u32 surface_height)
{
    if (surface_width > 0xffff || surface_height > 0xffff)
        return {};
    if (area.width == 0 || area.height == 0 || area.width > 0xffff || area.height > 0xffff)
        return {};
    if (area.width > surface_width || area.height > surface_height)
        return {};
    if (area.source_x > surface_width - area.width || area.destination_x > surface_width - area.width
        || area.source_y > surface_height - area.height || area.destination_y > surface_height - area.height)
        return {};

    u32 source_x = area.source_x;
    u32 source_y = area.source_y;
    u32 destination_x = area.destination_x;
    u32 destination_y = area.destination_y;
    u32 direction = 0;

    // Start at the opposite edge when source and destination overlap, as in
    // Linux radeonfb's copyarea path.
    if (destination_x <= source_x)
        direction |= 1;
    else {
        source_x += area.width - 1;
        destination_x += area.width - 1;
    }
    if (destination_y <= source_y)
        direction |= 2;
    else {
        source_y += area.height - 1;
        destination_y += area.height - 1;
    }
    if (source_x > 0xffff || destination_x > 0xffff || source_y > 0xffff || destination_y > 0xffff)
        return {};

    return BlitPlan {
        .direction = direction,
        .source_y_x = (source_y << 16) | source_x,
        .destination_y_x = (destination_y << 16) | destination_x,
        .height_width = (area.height << 16) | area.width,
    };
}

}
