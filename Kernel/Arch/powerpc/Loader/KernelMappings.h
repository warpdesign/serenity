/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>
#include <Kernel/Arch/powerpc/Loader/KernelImagePlan.h>

namespace Kernel::PowerPC::Loader {

struct KernelMappings {
    u32 table_physical { 0 };
    u32 table_virtual { 0 };
    u32 group_count { 0 };
    u32 mapped_pages { 0 };
};

bool map_kernel_image(u8 const* image, size_t image_size, KernelImagePlan const&, u32 physical_base,
    u32 runtime_base, u32 table_physical, u32 table_size, KernelMappings&);

}
