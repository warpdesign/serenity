/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Platform.h>
#include <AK/Types.h>
#include <Kernel/Memory/PhysicalAddress.h>

VALIDATE_IS_POWERPC32()

namespace Kernel {

struct ArchSpecificBootInfo {
    FlatPtr open_firmware_entry { 0 };
    FlatPtr hpt_virtual_address { 0 };
    PhysicalAddress hpt_physical_address { 0 };
    u32 hpt_group_count { 0 };
};

}
