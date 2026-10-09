/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibELF/Image.h>
#include <LibTest/TestCase.h>

TEST_CASE(native_image_field_decoding)
{
    alignas(8) u8 bytes[0x1004] {};
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
    header.e_machine = ELFSIZE == 32 ? EM_PPC : EM_X86_64;
    header.e_version = EV_CURRENT;
    header.e_entry = 0x1000;
    header.e_ehsize = sizeof(header);
    header.e_phoff = sizeof(header);
    header.e_phnum = 2;
    header.e_phentsize = sizeof(Elf_Phdr);
    header.e_shoff = sizeof(header) + 2 * sizeof(Elf_Phdr);
    header.e_shnum = 1;
    header.e_shentsize = sizeof(Elf_Shdr);

    Elf_Phdr executable_program {};
    executable_program.p_type = PT_LOAD;
    executable_program.p_offset = 0x1000;
    executable_program.p_vaddr = 0x1000;
    executable_program.p_filesz = 4;
    executable_program.p_memsz = 4;
    executable_program.p_flags = PF_R | PF_X;
    executable_program.p_align = 0x1000;
    Elf_Phdr read_only_program = executable_program;
    read_only_program.p_vaddr = 0x2000;
    read_only_program.p_flags = PF_R;

    Elf_Shdr executable_section {};
    executable_section.sh_type = SHT_PROGBITS;
    executable_section.sh_flags = SHF_ALLOC | SHF_EXECINSTR;
    executable_section.sh_offset = 0x1000;
    executable_section.sh_size = 4;
    executable_section.sh_addralign = 4;

    __builtin_memcpy(bytes, &header, sizeof(header));
    __builtin_memcpy(bytes + header.e_phoff, &executable_program, sizeof(executable_program));
    __builtin_memcpy(bytes + header.e_phoff + sizeof(executable_program), &read_only_program, sizeof(read_only_program));
    __builtin_memcpy(bytes + header.e_shoff, &executable_section, sizeof(executable_section));

    ELF::Image image { ReadonlyBytes { bytes, sizeof(bytes) }, false };
    EXPECT(image.is_valid());
    if (!image.is_valid())
        return;
    EXPECT(image.program_header(0).is_executable());
    EXPECT(!image.program_header(1).is_executable());
    EXPECT(image.program_header(1).is_readable());
    EXPECT(image.section(0).is_executable());

    Elf_Sym raw_symbol {};
    raw_symbol.st_info = ELF_ST_INFO(STB_GLOBAL, STT_FUNC);
    ELF::Image::Symbol symbol { image, 0, raw_symbol };
    EXPECT_EQ(symbol.type(), static_cast<unsigned>(STT_FUNC));
    EXPECT_EQ(symbol.bind(), static_cast<unsigned>(STB_GLOBAL));

    Elf_Rela raw_relocation {};
    raw_relocation.r_offset = 0x1000;
    raw_relocation.r_info = ELF_R_INFO(7ull, 5);
    raw_relocation.r_addend = 17;
    ELF::Image::Relocation relocation { image, raw_relocation, true };
    EXPECT_EQ(relocation.symbol_index(), 7u);
    EXPECT_EQ(relocation.type(), 5u);
    EXPECT_EQ(relocation.addend(), 17u);
}
