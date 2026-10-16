/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibELF/ELFABI.h>

namespace ELF {

constexpr char const* handled_dynamic_tag_name(Elf_Sword tag)
{
    return tag == DT_PPC_GOT ? "PPC_GOT" : nullptr;
}

}
