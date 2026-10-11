/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/Loader/KernelImagePlan.h>
#include <LibELF/ELFABI.h>

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

}
