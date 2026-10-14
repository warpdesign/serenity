/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Devices/GPU/ATI/RadeonDefinitions.h>

using namespace Kernel::Radeon;

static_assert(surface_control_for_linear_32bpp(ApertureSwap::SurfaceControl, false)
    == (SurfaceControl::Aperture0Swap32BPP | SurfaceControl::Aperture1Swap32BPP));
static_assert((surface_control_for_linear_32bpp(ApertureSwap::SurfaceControl, false)
    & SurfaceControl::TranslationDisable) == 0);
static_assert(surface_control_for_linear_32bpp(ApertureSwap::ConfigControl, false)
    == SurfaceControl::TranslationDisable);
static_assert(surface_control_for_linear_32bpp(ApertureSwap::SurfaceControl, true)
    == SurfaceControl::TranslationDisable);

int main()
{
    if (Register::Surface0UpperBound - Register::Surface0LowerBound != 4)
        return 1;
    if (Register::Surface0Info - Register::Surface0LowerBound != 8)
        return 1;
    if (surface_control_for_linear_32bpp(ApertureSwap::SurfaceControl, false) != 0x00a00000)
        return 1;
    return 0;
}
