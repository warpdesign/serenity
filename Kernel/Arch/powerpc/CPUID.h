/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/ArbitrarySizedEnum.h>
#include <AK/Platform.h>
#include <AK/StringView.h>
#include <AK/Types.h>

VALIDATE_IS_POWERPC32()

namespace Kernel {

AK_MAKE_ARBITRARY_SIZED_ENUM(CPUFeature, u32,
    FPU = CPUFeature(1u) << 0u,
    HashedMMU = CPUFeature(1u) << 1u,
    AltiVec = CPUFeature(1u) << 2u)

namespace PowerPC {

inline u32 read_pvr()
{
    u32 value;
    asm volatile("mfspr %0,287" : "=r"(value));
    return value;
}

StringView processor_model_name(u32 pvr);

}

CPUFeature::Type detect_cpu_features(u32 pvr);

}
