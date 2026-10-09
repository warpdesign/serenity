/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Platform.h>

VALIDATE_IS_POWERPC32()

namespace Kernel {

// The PowerPC thread pointer lives in r2, which is part of ThreadRegisters.
struct ArchSpecificThreadData {
};

}
