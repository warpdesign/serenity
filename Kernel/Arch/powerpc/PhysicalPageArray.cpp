/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/PageDirectoryBackend.h>
#include <Kernel/Arch/powerpc/PhysicalPageArray.h>

static bool valid_range(PowerPCHashPageTable const* table, unsigned int virtual_base, unsigned int page_count)
{
    return table && table->entries && page_count != 0 && virtual_base >= 0xc0000000u
        && (virtual_base & (POWERPC_HPT_PAGE_SIZE - 1)) == 0
        && static_cast<unsigned long long>(virtual_base) + static_cast<unsigned long long>(page_count) * POWERPC_HPT_PAGE_SIZE <= 0x100000000ull;
}

int powerpc_map_kernel_page_array(PowerPCHashPageTable const* table, unsigned int virtual_base, unsigned int physical_base, unsigned int page_count)
{
    if (!valid_range(table, virtual_base, page_count)
        || (physical_base & (POWERPC_HPT_PAGE_SIZE - 1)) != 0
        || static_cast<unsigned long long>(physical_base) + static_cast<unsigned long long>(page_count) * POWERPC_HPT_PAGE_SIZE > 0x100000000ull)
        return 0;

    unsigned int mapped = 0;
    for (; mapped < page_count; ++mapped) {
        unsigned int address = virtual_base + mapped * POWERPC_HPT_PAGE_SIZE;
        unsigned int vsid = address >> 28;
        if (powerpc_hpt_find(table, vsid, address))
            break;

        Kernel::Memory::PageMappingRequest request {
            .physical_page_base = static_cast<unsigned long long>(physical_base) + mapped * POWERPC_HPT_PAGE_SIZE,
            .memory_type = Kernel::Memory::MemoryType::Normal,
            .writable = true,
            .user_allowed = false,
            .execute_disabled = false,
        };
        if (!powerpc_apply_kernel_mapping(table, address, request))
            break;
    }
    if (mapped == page_count)
        return 1;

    for (unsigned int index = 0; index < mapped; ++index) {
        unsigned int address = virtual_base + index * POWERPC_HPT_PAGE_SIZE;
        powerpc_hpt_unmap(table, address >> 28, address);
    }
    return 0;
}

int powerpc_unmap_kernel_page_array(PowerPCHashPageTable const* table, unsigned int virtual_base, unsigned int page_count)
{
    if (!valid_range(table, virtual_base, page_count))
        return 0;

    int success = 1;
    for (unsigned int index = 0; index < page_count; ++index) {
        unsigned int address = virtual_base + index * POWERPC_HPT_PAGE_SIZE;
        if (!powerpc_hpt_unmap(table, address >> 28, address))
            success = 0;
    }
    return success;
}
