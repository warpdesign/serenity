/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Platform.h>
#include <AK/Types.h>

VALIDATE_IS_POWERPC32()

namespace Kernel {

struct alignas(16) FPUState {
    u64 fpr[32];
    // mffs copies the 32-bit FPSCR into the low word of an FPR image.
    u64 fpscr_image;
};

static_assert(sizeof(FPUState) == 272);
static_assert(__builtin_offsetof(FPUState, fpscr_image) == 256);

extern "C" void store_fpu_state(FPUState*);
extern "C" void load_fpu_state(FPUState const*);

}
