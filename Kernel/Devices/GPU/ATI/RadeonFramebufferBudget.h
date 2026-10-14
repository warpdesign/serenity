/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

namespace Kernel::Radeon {

// The current driver only needs scanout memory. Mapping the whole BAR also
// commits an equally sized RAM buffer for fake framebuffer writes.
static constexpr unsigned long long maximum_framebuffer_mapping_bytes = 32ull * 1024 * 1024;

constexpr unsigned long long framebuffer_mapping_bytes(unsigned long long aperture_size)
{
    return aperture_size < maximum_framebuffer_mapping_bytes ? aperture_size : maximum_framebuffer_mapping_bytes;
}

}
