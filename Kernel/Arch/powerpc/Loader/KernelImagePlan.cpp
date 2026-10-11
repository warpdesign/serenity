/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/Loader/KernelImagePlan.h>
#include <LibELF/ELFABI.h>
#include <LibELF/Relocation.h>

namespace Kernel::PowerPC::Loader {

bool plan_kernel_image(u8 const* image, size_t image_size, KernelImagePlan& plan)
{
    plan = {};
    if (!image || image_size < sizeof(Elf_Ehdr))
        return false;
    Elf_Ehdr header;
    __builtin_memcpy(&header, image, sizeof(header));
    if (header.e_ident[EI_MAG0] != ELFMAG0 || header.e_ident[EI_MAG1] != ELFMAG1
        || header.e_ident[EI_MAG2] != ELFMAG2 || header.e_ident[EI_MAG3] != ELFMAG3
        || header.e_ident[EI_CLASS] != ELFCLASS32 || header.e_ident[EI_DATA] != ELFDATA2MSB
        || header.e_ident[EI_VERSION] != EV_CURRENT || header.e_type != ET_DYN
        || header.e_machine != EM_PPC || header.e_version != EV_CURRENT
        || header.e_ehsize != sizeof(Elf_Ehdr) || header.e_phentsize != sizeof(Elf_Phdr)
        || header.e_phnum == 0 || header.e_phnum > 16
        || header.e_phoff > image_size
        || static_cast<size_t>(header.e_phnum) * sizeof(Elf_Phdr) > image_size - header.e_phoff)
        return false;

    bool entry_is_executable = false;
    u32 segment_starts[16];
    u32 segment_ends[16];
    u32 first = 0xffffffffu;
    u32 end = 0;
    u32 count = 0;
    for (u32 index = 0; index < header.e_phnum; ++index) {
        Elf_Phdr segment;
        __builtin_memcpy(&segment, image + header.e_phoff + index * sizeof(Elf_Phdr), sizeof(segment));
        if (segment.p_type != PT_LOAD)
            continue;
        if (segment.p_memsz == 0 || segment.p_filesz > segment.p_memsz
            || segment.p_offset > image_size || segment.p_filesz > image_size - segment.p_offset
            || segment.p_align < 4096 || (segment.p_align & (segment.p_align - 1)) != 0
            || (segment.p_vaddr & 4095u) != 0 || (segment.p_offset & 4095u) != 0
            || (segment.p_vaddr & (segment.p_align - 1)) != (segment.p_offset & (segment.p_align - 1))
            || segment.p_paddr != segment.p_vaddr
            || segment.p_vaddr >= 0x10000000u || segment.p_memsz > 0x10000000u - segment.p_vaddr)
            return false;
        u32 segment_end = segment.p_vaddr + segment.p_memsz;
        for (u32 previous = 0; previous < count; ++previous) {
            if (segment.p_vaddr < segment_ends[previous] && segment_starts[previous] < segment_end)
                return false;
        }
        segment_starts[count] = segment.p_vaddr;
        segment_ends[count] = segment_end;
        if (header.e_entry >= segment.p_vaddr && header.e_entry < segment.p_vaddr + segment.p_filesz
            && (segment.p_flags & PF_X) != 0)
            entry_is_executable = true;
        if (segment.p_vaddr < first)
            first = segment.p_vaddr;
        if (segment_end > end)
            end = segment_end;
        ++count;
    }
    if (count == 0 || !entry_is_executable)
        return false;

    // The firmware loader reserves one contiguous physical span for all
    // PT_LOAD segments; the holes remain unmapped in the final kernel view.
    plan = { first, static_cast<u32>((end + 4095u) & ~4095u), header.e_entry, count };
    return true;
}

bool load_kernel_image(u8 const* image, size_t image_size, KernelImagePlan const& plan, u8* destination, size_t destination_size)
{
    KernelImagePlan checked;
    if (!plan_kernel_image(image, image_size, checked)
        || checked.first_virtual_offset != plan.first_virtual_offset
        || checked.end_virtual_offset != plan.end_virtual_offset
        || checked.entry_offset != plan.entry_offset
        || checked.load_segment_count != plan.load_segment_count
        || !destination || destination_size < plan.end_virtual_offset - plan.first_virtual_offset)
        return false;
    auto source_start = static_cast<u64>(reinterpret_cast<FlatPtr>(image));
    auto destination_start = static_cast<u64>(reinterpret_cast<FlatPtr>(destination));
    if (source_start < destination_start + destination_size
        && destination_start < source_start + image_size)
        return false;

    Elf_Ehdr header;
    __builtin_memcpy(&header, image, sizeof(header));
    for (u32 index = 0; index < header.e_phnum; ++index) {
        Elf_Phdr segment;
        __builtin_memcpy(&segment, image + header.e_phoff + index * sizeof(segment), sizeof(segment));
        if (segment.p_type != PT_LOAD)
            continue;
        auto* target = destination + segment.p_vaddr - plan.first_virtual_offset;
        __builtin_memcpy(target, image + segment.p_offset, segment.p_filesz);
        __builtin_memset(target + segment.p_filesz, 0, segment.p_memsz - segment.p_filesz);
    }
    return true;
}

static bool range_is_in_segment(Elf_Phdr const* segments, u32 segment_count, u32 start, u32 length, bool file_bytes_only)
{
    for (u32 index = 0; index < segment_count; ++index) {
        auto const& segment = segments[index];
        u32 limit = file_bytes_only ? segment.p_filesz : segment.p_memsz;
        if (start >= segment.p_vaddr && length <= limit
            && start - segment.p_vaddr <= limit - length)
            return true;
    }
    return false;
}

bool relocate_kernel_image(u8 const* image, size_t image_size, KernelImagePlan const& plan, u8* destination,
    size_t destination_size, u32 runtime_base, u32& relocation_count)
{
    relocation_count = 0;
    KernelImagePlan checked;
    if (!plan_kernel_image(image, image_size, checked)
        || checked.first_virtual_offset != plan.first_virtual_offset
        || checked.end_virtual_offset != plan.end_virtual_offset
        || checked.entry_offset != plan.entry_offset
        || checked.load_segment_count != plan.load_segment_count
        || !destination || destination_size < plan.end_virtual_offset - plan.first_virtual_offset
        || reinterpret_cast<FlatPtr>(destination) < plan.first_virtual_offset
        || runtime_base < KERNEL_MAPPING_BASE || (runtime_base & 4095u) != 0
        || runtime_base > 0xffffffffu - plan.end_virtual_offset)
        return false;

    Elf_Ehdr header;
    __builtin_memcpy(&header, image, sizeof(header));
    Elf_Phdr load_segments[16];
    u32 segment_count = 0;
    Elf_Phdr dynamic_segment {};
    bool has_dynamic_segment = false;
    for (u32 index = 0; index < header.e_phnum; ++index) {
        Elf_Phdr segment;
        __builtin_memcpy(&segment, image + header.e_phoff + index * sizeof(segment), sizeof(segment));
        if (segment.p_type == PT_LOAD)
            load_segments[segment_count++] = segment;
        else if (segment.p_type == PT_DYNAMIC) {
            if (has_dynamic_segment)
                return false;
            dynamic_segment = segment;
            has_dynamic_segment = true;
        }
    }
    if (!has_dynamic_segment || dynamic_segment.p_filesz < sizeof(Elf_Dyn)
        || dynamic_segment.p_filesz % sizeof(Elf_Dyn) != 0
        || !range_is_in_segment(load_segments, segment_count, dynamic_segment.p_vaddr, dynamic_segment.p_filesz, true))
        return false;

    u32 rela_address = 0;
    u32 rela_size = 0;
    u32 rela_entry_size = 0;
    u32 relative_count = 0;
    bool terminated = false;
    auto* dynamics = destination + dynamic_segment.p_vaddr - plan.first_virtual_offset;
    for (u32 index = 0; index < dynamic_segment.p_filesz / sizeof(Elf_Dyn); ++index) {
        Elf_Dyn entry;
        __builtin_memcpy(&entry, dynamics + index * sizeof(entry), sizeof(entry));
        if (entry.d_tag == DT_NULL) {
            terminated = true;
            break;
        }
        if (entry.d_tag == DT_RELA)
            rela_address = entry.d_un.d_ptr;
        else if (entry.d_tag == DT_RELASZ)
            rela_size = entry.d_un.d_val;
        else if (entry.d_tag == DT_RELAENT)
            rela_entry_size = entry.d_un.d_val;
        else if (entry.d_tag == DT_RELACOUNT)
            relative_count = entry.d_un.d_val;
        else if (entry.d_tag == DT_REL || entry.d_tag == DT_RELR)
            return false;
    }
    if (!terminated || rela_address == 0 || rela_size == 0 || rela_entry_size != sizeof(Elf_Rela)
        || rela_size % sizeof(Elf_Rela) != 0 || relative_count == 0
        || relative_count > rela_size / sizeof(Elf_Rela)
        || !range_is_in_segment(load_segments, segment_count, rela_address, rela_size, true))
        return false;

    auto* relocations = destination + rela_address - plan.first_virtual_offset;
    for (u32 index = 0; index < relative_count; ++index) {
        Elf_Rela relocation;
        __builtin_memcpy(&relocation, relocations + index * sizeof(relocation), sizeof(relocation));
        if (ELF_R_TYPE(relocation.r_info) != R_PPC_RELATIVE || ELF_R_SYM(relocation.r_info) != 0
            || (relocation.r_offset & 3u) != 0
            || !range_is_in_segment(load_segments, segment_count, relocation.r_offset, sizeof(u32), false)
            || (relocation.r_offset < rela_address + rela_size && rela_address < relocation.r_offset + sizeof(u32)))
            return false;
        i64 value = static_cast<i64>(runtime_base) + relocation.r_addend;
        if (value < 0 || value > 0xffffffffll)
            return false;
    }

    auto physical_base = reinterpret_cast<FlatPtr>(destination) - plan.first_virtual_offset;
    if (!ELF::perform_relative_relocations(physical_base, runtime_base,
            reinterpret_cast<FlatPtr>(dynamics)))
        return false;
    relocation_count = relative_count;
    return true;
}

}
