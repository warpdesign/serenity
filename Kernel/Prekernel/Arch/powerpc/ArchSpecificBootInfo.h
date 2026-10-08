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

struct ArchSpecificBootInfo {
    FlatPtr open_firmware_entry { 0 };
};

}
