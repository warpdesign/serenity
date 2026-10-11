/*
 * Copyright (c) 2022-2026, Liav A. <liavalb@hotmail.co.il>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/CPUInfo.h>
#include <Kernel/FileSystem/SysFS/Subsystems/Kernel/CPUInfo.h>
#include <Kernel/Sections.h>

namespace Kernel {

UNMAP_AFTER_INIT SysFSCPUInformation::SysFSCPUInformation(SysFSDirectory const& parent_directory)
    : SysFSGlobalInformation(parent_directory)
{
}

UNMAP_AFTER_INIT NonnullRefPtr<SysFSCPUInformation> SysFSCPUInformation::must_create(SysFSDirectory const& parent_directory)
{
    return adopt_ref_if_nonnull(new (nothrow) SysFSCPUInformation(parent_directory)).release_nonnull();
}

ErrorOr<void> SysFSCPUInformation::try_generate(KBufferBuilder& builder)
{
    return try_generate_cpu_info(builder);
}

}
