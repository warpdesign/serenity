/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Devices/GPU/ATI/RadeonFramebufferBudget.h>

using namespace Kernel;

static_assert(Radeon::framebuffer_mapping_bytes(128ull * 1024 * 1024) == 32ull * 1024 * 1024);
static_assert(Radeon::framebuffer_mapping_bytes(8ull * 1024 * 1024) == 8ull * 1024 * 1024);

int main(int argc, char**)
{
    if (argc == 1)
        return 0;
    if (Radeon::framebuffer_mapping_bytes(16ull * 1024 * 1024) != 16ull * 1024 * 1024)
        return 1;
    if (Radeon::framebuffer_mapping_bytes(64ull * 1024 * 1024) != 32ull * 1024 * 1024)
        return 1;
    return 0;
}
