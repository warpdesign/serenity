/*
 * Copyright (c) 2022, Liav A. <liavalb@hotmail.co.il>
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/CPUInfo.h>
#include <Kernel/Debug.h>
#include <Kernel/Library/KBufferBuilder.h>

namespace Kernel {

ErrorOr<void> try_generate_cpu_info(KBufferBuilder&)
{
    dmesgln("TODO: Implement ProcessorInfo for AArch64!");
    return Error::from_errno(EINVAL);
}

}
