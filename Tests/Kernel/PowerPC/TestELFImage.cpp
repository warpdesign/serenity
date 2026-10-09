/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibELF/Image.h>

extern "C" int powerpc_native_elf_image_test()
{
    alignas(4) unsigned char bytes[0x1004] {};
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
    header.e_entry = 0x1000;
    header.e_ehsize = sizeof(header);
    header.e_phoff = sizeof(header);
    header.e_phnum = 2;
    header.e_phentsize = sizeof(Elf_Phdr);
    header.e_shoff = sizeof(header) + 2 * sizeof(Elf_Phdr);
    header.e_shnum = 1;
    header.e_shentsize = sizeof(Elf_Shdr);

    Elf_Phdr program {};
    program.p_type = PT_LOAD;
    program.p_offset = 0x1000;
    program.p_vaddr = 0x1000;
    program.p_filesz = 4;
    program.p_memsz = 4;
    program.p_flags = PF_R | PF_X;
    program.p_align = 0x1000;
    Elf_Phdr read_only_program = program;
    read_only_program.p_vaddr = 0x2000;
    read_only_program.p_flags = PF_R;

    Elf_Shdr section {};
    section.sh_type = SHT_PROGBITS;
    section.sh_flags = SHF_ALLOC | SHF_EXECINSTR;
    section.sh_offset = 0x1000;
    section.sh_size = 4;
    section.sh_addralign = 4;

    __builtin_memcpy(bytes, &header, sizeof(header));
    __builtin_memcpy(bytes + header.e_phoff, &program, sizeof(program));
    __builtin_memcpy(bytes + header.e_phoff + sizeof(program), &read_only_program, sizeof(read_only_program));
    __builtin_memcpy(bytes + header.e_shoff, &section, sizeof(section));
    ELF::Image image { ReadonlyBytes { bytes, sizeof(bytes) }, false };
    if (!image.is_valid() || image.machine() != EM_PPC || image.entry().get() != 0x1000)
        return 0;
    auto loaded_program = image.program_header(0);
    if (loaded_program.type() != PT_LOAD || loaded_program.vaddr().get() != 0x1000
        || loaded_program.size_in_image() != 4 || !loaded_program.is_executable()
        || image.program_header(1).is_executable() || !image.program_header(1).is_readable())
        return 0;
    auto loaded_section = image.section(0);
    if (!loaded_section.is_executable() || loaded_section.size() != 4)
        return 0;
    auto machine_name = ELF::Image::object_machine_type_to_string(EM_PPC);
    if (!machine_name.has_value() || machine_name.value() != "PowerPC"sv)
        return 0;

    Elf_Sym raw_symbol {};
    raw_symbol.st_info = ELF_ST_INFO(STB_GLOBAL, STT_FUNC);
    ELF::Image::Symbol symbol { image, 0, raw_symbol };
    if (symbol.type() != STT_FUNC || symbol.bind() != STB_GLOBAL)
        return 0;

    Elf_Rela raw_relocation {};
    raw_relocation.r_offset = 0x1000;
    raw_relocation.r_info = ELF_R_INFO(7ull, 5);
    raw_relocation.r_addend = 17;
    ELF::Image::Relocation relocation { image, raw_relocation, true };
    return relocation.offset() == 0x1000 && relocation.type() == 5
        && relocation.symbol_index() == 7 && relocation.addend() == 17;
}
