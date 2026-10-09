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
    TLS_DTPMOD = R_PPC_DTPMOD32,
    TLS_TPREL = R_PPC_TPREL32,
    TLS_DTPREL = R_PPC_DTPREL32,
    IRELATIVE = R_PPC_IRELATIVE,
    // PowerPC32 has no TLSDESC relocation. Keep this outside the ELF32 relocation range.
    TLSDESC = 0xffffffffu,
};

}
