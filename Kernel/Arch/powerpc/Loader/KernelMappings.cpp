/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/HashPageTable.h>
#include <Kernel/Arch/powerpc/Loader/KernelMappings.h>
#include <LibELF/ELFABI.h>

namespace Kernel::PowerPC::Loader {

bool map_kernel_image(u8 const* image, size_t image_size, KernelImagePlan const& plan, u32 physical_base,
    u32 runtime_base, u32 table_physical, u32 table_size, KernelMappings& output)
{
    output = {};
    KernelImagePlan checked;
    if (!plan_kernel_image(image, image_size, checked)
        || checked.first_virtual_offset != plan.first_virtual_offset
        || checked.end_virtual_offset != plan.end_virtual_offset
        || checked.entry_offset != plan.entry_offset
        || checked.load_segment_count != plan.load_segment_count
        || table_size < 64 * 1024 || table_size > 32 * 1024 * 1024
        || (table_size & (table_size - 1)) != 0 || (table_physical & (table_size - 1)) != 0
        || table_physical > 0xffffffffu - table_size
        || physical_base > 0xffffffffu - (plan.end_virtual_offset - plan.first_virtual_offset)
        || runtime_base > 0xffffffffu - plan.end_virtual_offset)
        return false;
    u64 table_virtual = static_cast<u64>(KERNEL_MAPPING_BASE) + table_physical;
    u64 kernel_physical_end = static_cast<u64>(physical_base) + plan.end_virtual_offset - plan.first_virtual_offset;
    u64 kernel_virtual_start = static_cast<u64>(runtime_base) + plan.first_virtual_offset;
    if (table_virtual > 0xffffffffu - table_size
        || (table_physical < kernel_physical_end && physical_base < static_cast<u64>(table_physical) + table_size)
        || (table_virtual < static_cast<u64>(runtime_base) + plan.end_virtual_offset
            && kernel_virtual_start < table_virtual + table_size))
        return false;

    auto* entries = reinterpret_cast<PowerPCHPTEntry*>(table_physical);
    __builtin_memset(entries, 0, table_size);
    PowerPCHashPageTable table { entries, table_size / 64 };
    Elf_Ehdr header;
    __builtin_memcpy(&header, image, sizeof(header));
    u32 mapped_pages = 0;
    for (u32 index = 0; index < header.e_phnum; ++index) {
        Elf_Phdr segment;
        __builtin_memcpy(&segment, image + header.e_phoff + index * sizeof(segment), sizeof(segment));
        if (segment.p_type != PT_LOAD)
            continue;
        u32 lower_flags = POWERPC_HPT_REFERENCED | POWERPC_HPT_CHANGED | POWERPC_HPT_COHERENT
            | ((segment.p_flags & PF_W) ? POWERPC_HPT_SUPERVISOR_ONLY : POWERPC_HPT_READ_ONLY);
        for (u32 offset = 0; offset < segment.p_memsz; offset += POWERPC_HPT_PAGE_SIZE) {
            u32 effective = runtime_base + segment.p_vaddr + offset;
            u32 physical = physical_base + (segment.p_vaddr - plan.first_virtual_offset) + offset;
            if (!powerpc_hpt_map(&table, effective >> 28, effective, physical, lower_flags))
                return false;
            ++mapped_pages;
        }
    }

    u32 self_flags = POWERPC_HPT_REFERENCED | POWERPC_HPT_CHANGED | POWERPC_HPT_COHERENT | POWERPC_HPT_SUPERVISOR_ONLY;
    for (u32 offset = 0; offset < table_size; offset += POWERPC_HPT_PAGE_SIZE) {
        u32 effective = static_cast<u32>(table_virtual) + offset;
        if (!powerpc_hpt_map(&table, effective >> 28, effective, table_physical + offset, self_flags))
            return false;
    }
    __asm__ volatile("sync" : : : "memory");
    output = { table_physical, static_cast<u32>(table_virtual), table.group_count, mapped_pages };
    return true;
}

}
