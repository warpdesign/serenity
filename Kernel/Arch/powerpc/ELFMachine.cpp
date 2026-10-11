/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/ELFMachine.h>
#include <LibELF/ELFABI.h>

namespace Kernel {

u16 native_elf_machine()
{
    return EM_PPC;
}

}
