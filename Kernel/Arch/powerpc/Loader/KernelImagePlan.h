/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>

namespace Kernel::PowerPC::Loader {

struct KernelImagePlan {
    u32 first_virtual_offset { 0 };
    u32 end_virtual_offset { 0 };
    u32 entry_offset { 0 };
    u32 load_segment_count { 0 };
};

bool plan_kernel_image(u8 const* image, size_t image_size, KernelImagePlan&);
bool load_kernel_image(u8 const* image, size_t image_size, KernelImagePlan const&, u8* destination, size_t destination_size);
bool relocate_kernel_image(u8 const* image, size_t image_size, KernelImagePlan const&, u8* destination,
    size_t destination_size, u32 runtime_base, u32& relocation_count);

}
