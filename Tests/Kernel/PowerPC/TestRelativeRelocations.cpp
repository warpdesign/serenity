/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibELF/ELFABI.h>
#include <LibELF/Relocation.h>

static_assert(sizeof(Elf_Dyn) == 8);
static_assert(sizeof(Elf_Rel) == 8);
static_assert(sizeof(Elf_Rela) == 12);
static_assert(R_PPC_RELATIVE == 22);

static FlatPtr read_word(unsigned char const* address)
{
    FlatPtr value;
    __builtin_memcpy(&value, address, sizeof(value));
    return value;
}

extern "C" int powerpc_relative_relocations_test()
{
    alignas(4) unsigned char bytes[512] {};
    FlatPtr base = reinterpret_cast<FlatPtr>(bytes);
    constexpr FlatPtr runtime_base = 0x40000000u;
    auto* dynamics = reinterpret_cast<Elf_Dyn*>(bytes);

    dynamics[0].d_tag = DT_RELA;
    dynamics[0].d_un.d_ptr = 0x60;
    dynamics[1].d_tag = DT_RELASZ;
    dynamics[1].d_un.d_val = 2 * sizeof(Elf_Rela);
    dynamics[2].d_tag = DT_RELAENT;
    dynamics[2].d_un.d_val = sizeof(Elf_Rela);
    dynamics[3].d_tag = DT_RELACOUNT;
    dynamics[3].d_un.d_val = 2;
    auto* rela = reinterpret_cast<Elf_Rela*>(bytes + 0x60);
    rela[0].r_offset = 0xc0;
    rela[0].r_info = ELF_R_INFO(0ull, R_PPC_RELATIVE);
    rela[0].r_addend = 0x1234;
    rela[1].r_offset = 0xc4;
    rela[1].r_info = ELF_R_INFO(0ull, R_PPC_RELATIVE);
    rela[1].r_addend = -4;
    if (!ELF::perform_relative_relocations(base, runtime_base, base)
        || read_word(bytes + 0xc0) != runtime_base + 0x1234
        || read_word(bytes + 0xc4) != runtime_base - 4)
        return 0;

    __builtin_memset(bytes, 0, sizeof(bytes));
    dynamics[0].d_tag = DT_REL;
    dynamics[0].d_un.d_ptr = 0x60;
    dynamics[1].d_tag = DT_RELSZ;
    dynamics[1].d_un.d_val = sizeof(Elf_Rel);
    dynamics[2].d_tag = DT_RELENT;
    dynamics[2].d_un.d_val = sizeof(Elf_Rel);
    dynamics[3].d_tag = DT_RELCOUNT;
    dynamics[3].d_un.d_val = 1;
    auto* rel = reinterpret_cast<Elf_Rel*>(bytes + 0x60);
    rel->r_offset = 0xc0;
    rel->r_info = ELF_R_INFO(0ull, R_PPC_RELATIVE);
    FlatPtr implicit_addend = 0x1234;
    __builtin_memcpy(bytes + 0xc0, &implicit_addend, sizeof(implicit_addend));
    if (!ELF::perform_relative_relocations(base, runtime_base, base)
        || read_word(bytes + 0xc0) != runtime_base + implicit_addend)
        return 0;

    __builtin_memset(bytes, 0, sizeof(bytes));
    dynamics[0].d_tag = DT_RELR;
    dynamics[0].d_un.d_ptr = 0x60;
    dynamics[1].d_tag = DT_RELRSZ;
    dynamics[1].d_un.d_val = 2 * sizeof(Elf_Relr);
    dynamics[2].d_tag = DT_RELRENT;
    dynamics[2].d_un.d_val = sizeof(Elf_Relr);
    auto* relr = reinterpret_cast<Elf_Relr*>(bytes + 0x60);
    relr[0] = 0xc0;
    relr[1] = 5;
    FlatPtr first = 0x11;
    FlatPtr second = 0x22;
    __builtin_memcpy(bytes + 0xc0, &first, sizeof(first));
    __builtin_memcpy(bytes + 0xc8, &second, sizeof(second));
    if (!ELF::perform_relative_relocations(base, runtime_base, base)
        || read_word(bytes + 0xc0) != runtime_base + first
        || read_word(bytes + 0xc8) != runtime_base + second)
        return 0;

    __builtin_memset(bytes, 0, sizeof(bytes));
    auto* header = reinterpret_cast<Elf_Ehdr*>(bytes);
    header->e_phoff = sizeof(Elf_Ehdr);
    header->e_phnum = 1;
    auto* program = reinterpret_cast<Elf_Phdr*>(bytes + header->e_phoff);
    program->p_type = PT_DYNAMIC;
    program->p_vaddr = 0x100;
    dynamics = reinterpret_cast<Elf_Dyn*>(bytes + 0x100);
    dynamics[0].d_tag = DT_RELA;
    dynamics[0].d_un.d_ptr = 0x180;
    dynamics[1].d_tag = DT_RELASZ;
    dynamics[1].d_un.d_val = sizeof(Elf_Rela);
    dynamics[2].d_tag = DT_RELAENT;
    dynamics[2].d_un.d_val = sizeof(Elf_Rela);
    dynamics[3].d_tag = DT_RELACOUNT;
    dynamics[3].d_un.d_val = 1;
    rela = reinterpret_cast<Elf_Rela*>(bytes + 0x180);
    rela[0].r_offset = 0x1c0;
    rela[0].r_info = ELF_R_INFO(0ull, R_PPC_RELATIVE);
    rela[0].r_addend = 0x1234;
    return ELF::perform_relative_relocations(base)
        && read_word(bytes + 0x1c0) == base + 0x1234;
}
