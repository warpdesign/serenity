/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/FPUState.h>
#include <fenv.h>

static_assert(sizeof(Kernel::FPUState) == 272);
static_assert(__builtin_offsetof(Kernel::FPUState, fpscr_image) == 256);

extern "C" int powerpc_fpu_state_test()
{
    Kernel::FPUState original {};
    Kernel::store_fpu_state(&original);

    alignas(8) static u64 const pattern[] {
        0x3ff0000000000000ull,
        0x4008000000000000ull,
        0x4014000000000000ull,
    };
    alignas(8) static u64 const replacement[] {
        0x4020000000000000ull,
        0x4022000000000000ull,
        0x4024000000000000ull,
    };

    bool passed = fesetround(FE_DOWNWARD) == 0;
    asm volatile("lfd 0,0(%0); lfd 14,8(%0); lfd 31,16(%0)" : : "r"(pattern) : "fr0", "fr14", "fr31", "memory");

    Kernel::FPUState saved {};
    Kernel::FPUState after_store {};
    Kernel::store_fpu_state(&saved);
    Kernel::store_fpu_state(&after_store);
    passed &= saved.fpr[0] == pattern[0] && saved.fpr[14] == pattern[1] && saved.fpr[31] == pattern[2]
        && after_store.fpr[0] == pattern[0] && (saved.fpscr_image & 3u) == 3u;

    passed &= fesetround(FE_UPWARD) == 0;
    asm volatile("lfd 0,0(%0); lfd 14,8(%0); lfd 31,16(%0)" : : "r"(replacement) : "fr0", "fr14", "fr31", "memory");
    Kernel::load_fpu_state(&saved);

    Kernel::FPUState restored {};
    Kernel::store_fpu_state(&restored);
    for (unsigned index = 0; index < 32; ++index)
        passed &= restored.fpr[index] == saved.fpr[index];
    passed &= static_cast<u32>(restored.fpscr_image) == static_cast<u32>(saved.fpscr_image)
        && fegetround() == FE_DOWNWARD;

    Kernel::load_fpu_state(&original);
    return passed;
}
