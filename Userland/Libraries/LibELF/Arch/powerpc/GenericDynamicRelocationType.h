/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Platform.h>
#include <LibELF/ELFABI.h>

VALIDATE_IS_POWERPC32()

namespace ELF {

enum class GenericDynamicRelocationType : unsigned {
    NONE = R_PPC_NONE,
    ABSOLUTE = R_PPC_ADDR32,
    COPY = R_PPC_COPY,
    GLOB_DAT = R_PPC_GLOB_DAT,
    JUMP_SLOT = R_PPC_JMP_SLOT,
    RELATIVE = R_PPC_RELATIVE,
};

}
