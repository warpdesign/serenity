/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibELF/Arch/GenericDynamicRelocationType.h>
#include <LibELF/ELFABI.h>
#include <LibELF/Relocation.h>
#include <LibTest/TestCase.h>

static FlatPtr read_word(u8 const* address)
{
    FlatPtr value;
    __builtin_memcpy(&value, address, sizeof(value));
    return value;
}

TEST_CASE(native_relative_relocations)
{
    alignas(8) u8 bytes[512] {};
    FlatPtr base = reinterpret_cast<FlatPtr>(bytes);
    constexpr FlatPtr runtime_base = 0x40000000u;
    auto* dynamics = reinterpret_cast<Elf_Dyn*>(bytes);

    dynamics[0].d_tag = DT_RELA;
    dynamics[0].d_un.d_ptr = 0x80;
    dynamics[1].d_tag = DT_RELASZ;
    dynamics[1].d_un.d_val = 2 * sizeof(Elf_Rela);
    dynamics[2].d_tag = DT_RELAENT;
    dynamics[2].d_un.d_val = sizeof(Elf_Rela);
    dynamics[3].d_tag = DT_RELACOUNT;
    dynamics[3].d_un.d_val = 2;
    auto* rela = reinterpret_cast<Elf_Rela*>(bytes + 0x80);
    rela[0].r_offset = 0xc0;
    rela[0].r_info = ELF_R_INFO(0ull, static_cast<unsigned>(ELF::GenericDynamicRelocationType::RELATIVE));
    rela[0].r_addend = 0x1234;
    rela[1].r_offset = 0xc0 + sizeof(FlatPtr);
    rela[1].r_info = ELF_R_INFO(0ull, static_cast<unsigned>(ELF::GenericDynamicRelocationType::RELATIVE));
    rela[1].r_addend = -4;
    EXPECT(ELF::perform_relative_relocations(base, runtime_base, base));
    EXPECT_EQ(read_word(bytes + 0xc0), runtime_base + 0x1234);
    EXPECT_EQ(read_word(bytes + 0xc0 + sizeof(FlatPtr)), runtime_base - 4);

    __builtin_memset(bytes, 0, sizeof(bytes));
    dynamics[0].d_tag = DT_REL;
    dynamics[0].d_un.d_ptr = 0x80;
    dynamics[1].d_tag = DT_RELSZ;
    dynamics[1].d_un.d_val = sizeof(Elf_Rel);
    dynamics[2].d_tag = DT_RELENT;
    dynamics[2].d_un.d_val = sizeof(Elf_Rel);
    dynamics[3].d_tag = DT_RELCOUNT;
    dynamics[3].d_un.d_val = 1;
    auto* rel = reinterpret_cast<Elf_Rel*>(bytes + 0x80);
    rel->r_offset = 0xc0;
    rel->r_info = ELF_R_INFO(0ull, static_cast<unsigned>(ELF::GenericDynamicRelocationType::RELATIVE));
    FlatPtr implicit_addend = 0x1234;
    __builtin_memcpy(bytes + 0xc0, &implicit_addend, sizeof(implicit_addend));
    EXPECT(ELF::perform_relative_relocations(base, runtime_base, base));
    EXPECT_EQ(read_word(bytes + 0xc0), runtime_base + implicit_addend);

    __builtin_memset(bytes, 0, sizeof(bytes));
    dynamics[0].d_tag = DT_RELR;
    dynamics[0].d_un.d_ptr = 0x80;
    dynamics[1].d_tag = DT_RELRSZ;
    dynamics[1].d_un.d_val = 2 * sizeof(Elf_Relr);
    dynamics[2].d_tag = DT_RELRENT;
    dynamics[2].d_un.d_val = sizeof(Elf_Relr);
    auto* relr = reinterpret_cast<Elf_Relr*>(bytes + 0x80);
    relr[0] = 0xc0;
    relr[1] = 5;
    FlatPtr first = 0x11;
    FlatPtr second = 0x22;
    __builtin_memcpy(bytes + 0xc0, &first, sizeof(first));
    __builtin_memcpy(bytes + 0xc0 + 2 * sizeof(FlatPtr), &second, sizeof(second));
    EXPECT(ELF::perform_relative_relocations(base, runtime_base, base));
    EXPECT_EQ(read_word(bytes + 0xc0), runtime_base + first);
    EXPECT_EQ(read_word(bytes + 0xc0 + 2 * sizeof(FlatPtr)), runtime_base + second);

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
    rela[0].r_info = ELF_R_INFO(0ull, static_cast<unsigned>(ELF::GenericDynamicRelocationType::RELATIVE));
    rela[0].r_addend = 0x1234;
    EXPECT(ELF::perform_relative_relocations(base));
    EXPECT_EQ(read_word(bytes + 0x1c0), base + 0x1234);
}
