/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <Kernel/Arch/powerpc/HashPageTable.h>
#include <Kernel/Boot/BootInfo.h>

namespace Kernel::PowerPC {

// Validate the firmware handoff against SDR1 and produce the mapped HPT view.
bool boot_hpt_from_boot_info(BootInfo const&, PowerPCHashPageTable&);

}
