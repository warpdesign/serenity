/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Optional.h>
#include <AK/Types.h>

namespace Kernel {

// Firmware may leave the drive in PIO despite advertising UDMA5. Until its
// cable and controller setup has been verified, introduce DMA at UDMA2.
constexpr Optional<u8> choose_via_udma_mode(u8 supported_modes, u8 selected_modes, bool cable80, Optional<u8> requested_mode = {})
{
    if (supported_modes == 0)
        return {};

    // An explicit request can retest a faster mode without a new disk image.
    // Still require the drive to advertise it and the cable bit for UDMA3+.
    if (requested_mode.has_value()) {
        u8 mode = requested_mode.value();
        if (mode > 5 || (mode > 2 && !cable80) || !(supported_modes & (1u << mode)))
            return {};
        return mode;
    }

    u8 limit = cable80 ? 5 : 2;
    if (selected_modes == 0) {
        if (limit > 2)
            limit = 2;
    } else {
        for (int mode = 6; mode >= 0; --mode) {
            if (selected_modes & (1u << mode)) {
                if (mode < limit)
                    limit = mode;
                break;
            }
        }
    }
    for (int mode = limit; mode >= 0; --mode) {
        if (supported_modes & (1u << mode))
            return static_cast<u8>(mode);
    }
    return {};
}

}
