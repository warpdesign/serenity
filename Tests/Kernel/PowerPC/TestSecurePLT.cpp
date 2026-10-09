/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibELF/Arch/powerpc/SecurePLT.h>
#include <LibELF/Image.h>
#include <LibELF/Relocation.h>

extern "C" unsigned char _binary_secure_plt_probe_so_start[];
extern "C" unsigned char _binary_secure_plt_probe_so_end[];
extern "C" void _plt_trampoline();

alignas(65536) static unsigned char s_loaded_image[0x20000] {};
static FlatPtr* s_plt_slots[2];
static unsigned s_fixup_count[2];
static unsigned s_argument_count;
static unsigned s_object_token;

extern "C" int imported_function_target(int value, double fraction)
{
    if ((value == 11 && fraction == 3.5) || (value == 21 && fraction == 5.5)
        || (value == 32 && fraction == 8.5) || (value == 42 && fraction == 10.5))
        ++s_argument_count;
    return value + static_cast<int>(fraction * 2);
}

extern "C" FlatPtr _fixup_plt_entry(void* object, u32 relocation_offset)
{
    if (object != &s_object_token || relocation_offset % sizeof(Elf_Rela) != 0)
        return 0;
    auto relocation_index = relocation_offset / sizeof(Elf_Rela);
    if (relocation_index >= 2)
        return 0;
    ++s_fixup_count[relocation_index];
    FlatPtr target = reinterpret_cast<FlatPtr>(&imported_function_target);
    *s_plt_slots[relocation_index] = target;
    __asm__ volatile("li 5,0; fsub 1,1,1" : : : "r5", "fr1");
    return target;
}

static void sync_instruction_cache(FlatPtr start, size_t size)
{
    constexpr FlatPtr line_size = 32;
    FlatPtr first = start & ~(line_size - 1);
    FlatPtr end = (start + size + line_size - 1) & ~(line_size - 1);
    for (FlatPtr address = first; address < end; address += line_size)
        __asm__ volatile("dcbf 0,%0" : : "r"(address) : "memory");
    __asm__ volatile("sync" : : : "memory");
    for (FlatPtr address = first; address < end; address += line_size)
        __asm__ volatile("icbi 0,%0" : : "r"(address) : "memory");
    __asm__ volatile("sync; isync" : : : "memory");
}

extern "C" int powerpc_secure_plt_test()
{
    auto fixture_size = static_cast<size_t>(_binary_secure_plt_probe_so_end - _binary_secure_plt_probe_so_start);
    ELF::Image image { ReadonlyBytes { _binary_secure_plt_probe_so_start, fixture_size }, false };
    if (!image.is_valid() || !image.is_dynamic() || image.machine() != EM_PPC)
        return 2;

    bool loaded = true;
    image.for_each_program_header([&](ELF::Image::ProgramHeader header) {
        if (header.type() != PT_LOAD)
            return;
        auto offset = header.vaddr().get();
        if (offset > sizeof(s_loaded_image) || header.size_in_memory() > sizeof(s_loaded_image) - offset
            || header.offset() > fixture_size || header.size_in_image() > fixture_size - header.offset()) {
            loaded = false;
            return;
        }
        __builtin_memset(s_loaded_image + offset, 0, header.size_in_memory());
        __builtin_memcpy(s_loaded_image + offset, _binary_secure_plt_probe_so_start + header.offset(), header.size_in_image());
        if (header.is_executable())
            sync_instruction_cache(reinterpret_cast<FlatPtr>(s_loaded_image + offset), header.size_in_memory());
    });
    if (!loaded)
        return 3;

    auto dynamic_section = image.lookup_section(".dynamic"sv);
    auto relocation_section = image.lookup_section(".rela.plt"sv);
    if (!dynamic_section.has_value() || !relocation_section.has_value() || relocation_section->entry_count() != 2)
        return 4;

    FlatPtr got_offset = 0;
    FlatPtr plt_offset = 0;
    auto* entries = reinterpret_cast<Elf_Dyn const*>(dynamic_section->raw_data());
    for (size_t index = 0; index < dynamic_section->entry_count(); ++index) {
        if (entries[index].d_tag == DT_PPC_GOT)
            got_offset = entries[index].d_un.d_ptr;
        else if (entries[index].d_tag == DT_PLTGOT)
            plt_offset = entries[index].d_un.d_ptr;
    }
    auto* relocations = reinterpret_cast<Elf_Rela const*>(relocation_section->raw_data());
    if (!got_offset || !plt_offset || got_offset > sizeof(s_loaded_image) - 3 * sizeof(FlatPtr)
        || plt_offset > sizeof(s_loaded_image) - 2 * sizeof(FlatPtr)
        || relocations[0].r_offset != plt_offset || relocations[1].r_offset != plt_offset + sizeof(FlatPtr)
        || ELF_R_TYPE(relocations[0].r_info) != R_PPC_JMP_SLOT || ELF_R_TYPE(relocations[1].r_info) != R_PPC_JMP_SLOT)
        return 5;

    FlatPtr base = reinterpret_cast<FlatPtr>(s_loaded_image);
    if (!ELF::perform_relative_relocations(base, base, base + dynamic_section->address()))
        return 12;
    auto* got = reinterpret_cast<FlatPtr*>(s_loaded_image + got_offset);
    s_plt_slots[0] = reinterpret_cast<FlatPtr*>(s_loaded_image + plt_offset);
    s_plt_slots[1] = s_plt_slots[0] + 1;
    *s_plt_slots[0] += base;
    *s_plt_slots[1] += base;
    ELF::install_secure_plt_resolver(got, &s_object_token, &_plt_trampoline);

    FlatPtr call_offset = 0;
    FlatPtr second_call_offset = 0;
    image.for_each_symbol([&](ELF::Image::Symbol symbol) {
        if (symbol.name() == "call_imported_function"sv)
            call_offset = symbol.value();
        else if (symbol.name() == "call_second_imported_function"sv)
            second_call_offset = symbol.value();
    });
    if (!call_offset || call_offset >= sizeof(s_loaded_image) || !second_call_offset || second_call_offset >= sizeof(s_loaded_image))
        return 6;

    auto call_imported_function = reinterpret_cast<int (*)(int, double)>(base + call_offset);
    auto call_second_imported_function = reinterpret_cast<int (*)(int, double)>(base + second_call_offset);
    s_fixup_count[0] = 0;
    s_fixup_count[1] = 0;
    s_argument_count = 0;
    int first = call_imported_function(10, 2.0);
    int second = call_second_imported_function(30, 6.0);
    int third = call_imported_function(20, 4.0);
    int fourth = call_second_imported_function(40, 8.0);
    if (first != 25)
        return 7;
    if (second != 58 || third != 39 || fourth != 72)
        return 8;
    if (s_fixup_count[0] != 1 || s_fixup_count[1] != 1)
        return 9;
    if (s_argument_count != 4)
        return 10;
    if (*s_plt_slots[0] != reinterpret_cast<FlatPtr>(&imported_function_target)
        || *s_plt_slots[1] != reinterpret_cast<FlatPtr>(&imported_function_target))
        return 11;
    return 1;
}
