/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/JsonObjectSerializer.h>
#include <Kernel/Arch/CPUInfo.h>
#include <Kernel/Arch/Processor.h>
#include <Kernel/Arch/powerpc/CPUID.h>
#include <Kernel/Library/KBufferBuilder.h>

namespace Kernel {

ErrorOr<void> try_generate_cpu_info(KBufferBuilder& builder)
{
    auto array = TRY(JsonArraySerializer<>::try_create(builder));
    auto object = TRY(array.add_object());
    auto pvr = PowerPC::read_pvr();
    auto& processor = Processor::current();

    TRY(object.add("processor"sv, processor.id()));
    TRY(object.add("model"sv, PowerPC::processor_model_name(pvr)));
    TRY(object.add("pvr"sv, pvr));

    auto features = TRY(object.add_array("features"sv));
    if (processor.has_feature(CPUFeature::FPU))
        TRY(features.add("fpu"sv));
    if (processor.has_feature(CPUFeature::HashedMMU))
        TRY(features.add("hashed_mmu"sv));
    if (processor.has_feature(CPUFeature::AltiVec))
        TRY(features.add("altivec"sv));
    TRY(features.finish());

    TRY(object.finish());
    TRY(array.finish());
    return {};
}

}
