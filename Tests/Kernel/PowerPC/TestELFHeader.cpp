/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibELF/Validation.h>

static_assert(ELFSIZE == 32);
static_assert(ELFCLASS == ELFCLASS32);
static_assert(ELFDATA == ELFDATA2MSB);
static_assert(sizeof(Elf_Ehdr) == 52);
static_assert(sizeof(Elf_Phdr) == 32);
static_assert(sizeof(Elf_Shdr) == 40);

namespace AK {

void vdbg(StringView, TypeErasedFormatParams&, bool) { }

}

extern "C" int powerpc_native_elf_header_test()
{
    Elf_Ehdr header {};
    header.e_ident[EI_MAG0] = ELFMAG0;
    header.e_ident[EI_MAG1] = ELFMAG1;
    header.e_ident[EI_MAG2] = ELFMAG2;
    header.e_ident[EI_MAG3] = ELFMAG3;
    header.e_ident[EI_CLASS] = ELFCLASS;
    header.e_ident[EI_DATA] = ELFDATA;
    header.e_ident[EI_VERSION] = EV_CURRENT;
    header.e_ident[EI_OSABI] = ELFOSABI_SYSV;
    header.e_type = ET_EXEC;
    header.e_machine = EM_PPC;
    header.e_version = EV_CURRENT;
    header.e_ehsize = sizeof(header);
    header.e_shentsize = sizeof(Elf_Shdr);

    auto wrong_class = header;
    wrong_class.e_ident[EI_CLASS] = ELFCLASS64;
    auto wrong_byte_order = header;
    wrong_byte_order.e_ident[EI_DATA] = ELFDATA2LSB;
    auto wrong_machine = header;
    wrong_machine.e_machine = EM_X86_64;

    auto const* bytes = reinterpret_cast<unsigned char const*>(&header);
    return ELF::validate_elf_header(header, sizeof(header), false)
        && !ELF::validate_elf_header(wrong_class, sizeof(wrong_class), false)
        && !ELF::validate_elf_header(wrong_byte_order, sizeof(wrong_byte_order), false)
        && !ELF::validate_elf_header(wrong_machine, sizeof(wrong_machine), false)
        && IS_ELF(header) && header.e_ident[EI_CLASS] == ELFCLASS32
        && header.e_ident[EI_DATA] == ELFDATA2MSB
        && bytes[16] == 0 && bytes[17] == ET_EXEC
        && bytes[18] == 0 && bytes[19] == EM_PPC
        && bytes[20] == 0 && bytes[21] == 0 && bytes[22] == 0 && bytes[23] == EV_CURRENT;
}
