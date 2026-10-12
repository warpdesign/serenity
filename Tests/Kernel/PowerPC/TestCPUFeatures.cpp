/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/CPU.h>
#include <Kernel/Arch/CPUID.h>

extern "C" u32 powerpc_cpu_pvr()
{
    return Kernel::PowerPC::read_pvr();
}

extern "C" int powerpc_cpu_features_test()
{
    auto pvr = Kernel::PowerPC::read_pvr();
    auto features = Kernel::detect_cpu_features(pvr);
    if (!features.has_flag(Kernel::CPUFeature::FPU)
        || !features.has_flag(Kernel::CPUFeature::HashedMMU))
        return 0;

    auto msr = Kernel::PowerPC::read_msr();
    if ((msr & 0x2000u) == 0 || (msr & 0x4000u) != 0)
        return 0;

    auto g3_features = Kernel::detect_cpu_features(0x00082212u);
    auto g4_features = Kernel::detect_cpu_features(0x80030100u);
    return !g3_features.has_flag(Kernel::CPUFeature::AltiVec)
        && g3_features.has_flag(Kernel::CPUFeature::Doze)
        && g4_features.has_flag(Kernel::CPUFeature::AltiVec)
        && !g4_features.has_flag(Kernel::CPUFeature::Doze)
        && Kernel::PowerPC::processor_model_name(0x00082212u) == "PowerPC 750CXe v2.2"sv
        && Kernel::PowerPC::processor_model_name(0x00083311u) == "PowerPC 750CXe v3.1b"sv
        && Kernel::PowerPC::processor_model_name(0x80030100u) == "PowerPC 7447A"sv
        && Kernel::PowerPC::processor_model_name(0xffff0000u) == "Unknown PowerPC"sv;
}
