/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibELF/Validation.h>
#include <LibTest/TestCase.h>

TEST_CASE(native_elf_header_class_and_byte_order)
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
    header.e_type = ET_REL;
    header.e_machine = ELFSIZE == 32 ? EM_PPC : EM_X86_64;
    header.e_version = EV_CURRENT;
    header.e_ehsize = sizeof(header);
    header.e_shentsize = sizeof(Elf_Shdr);

    EXPECT(ELF::validate_elf_header(header, sizeof(header), false));

    auto wrong_class = header;
    wrong_class.e_ident[EI_CLASS] = ELFCLASS == ELFCLASS32 ? ELFCLASS64 : ELFCLASS32;
    EXPECT(!ELF::validate_elf_header(wrong_class, sizeof(wrong_class), false));

    auto wrong_byte_order = header;
    wrong_byte_order.e_ident[EI_DATA] = ELFDATA == ELFDATA2MSB ? ELFDATA2LSB : ELFDATA2MSB;
    EXPECT(!ELF::validate_elf_header(wrong_byte_order, sizeof(wrong_byte_order), false));

    auto wrong_machine = header;
    wrong_machine.e_machine = ELFSIZE == 32 ? EM_X86_64 : EM_PPC;
    EXPECT(!ELF::validate_elf_header(wrong_machine, sizeof(wrong_machine), false));
}
