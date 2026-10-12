/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/CPUID.h>

namespace Kernel {

StringView PowerPC::processor_model_name(u32 pvr)
{
    switch (pvr >> 16) {
    case 0x0008:
        return "PowerPC 750"sv;
    case 0x7000:
        return "PowerPC 750FX"sv;
    case 0x000c:
        return "PowerPC 7400"sv;
    case 0x8000:
        return "PowerPC 7450"sv;
    case 0x8001:
        return "PowerPC 7455"sv;
    case 0x8002:
        return "PowerPC 7457"sv;
    case 0x8003:
        return "PowerPC 7447A"sv;
    case 0x8004:
        return "PowerPC 7448"sv;
    case 0x800c:
        return "PowerPC 7410"sv;
    default:
        return "Unknown PowerPC"sv;
    }
}

CPUFeature::Type detect_cpu_features(u32 pvr)
{
    auto version = pvr >> 16;
    auto baseline = CPUFeature::FPU | CPUFeature::HashedMMU;

    switch (version) {
    case 0x0008: // 750, including IBM 750CX/CXe/CXr revisions.
    case 0x7000: // IBM 750FX.
        return baseline | CPUFeature::Doze;
    case 0x000c: // 7400.
    case 0x8000: // 7450.
    case 0x8001: // 7455.
    case 0x8002: // 7457.
    case 0x8003: // 7447A.
    case 0x8004: // 7448.
    case 0x800c: // 7410.
        return baseline | CPUFeature::AltiVec;
    default:
        return CPUFeature::Type(0u);
    }
}

}
