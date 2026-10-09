/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/PageDirectoryBackend.h>
#include <Kernel/Arch/powerpc/PhysicalPageArray.h>
#include <Kernel/Memory/PageMapping.h>
#include <Kernel/Sections.h>

extern "C" int powerpc_test_map_kernel_page(PowerPCHashPageTable const* table, unsigned int effective_address, unsigned int physical_address)
{
    Kernel::Memory::PageMappingRequest request {
        .physical_page_base = physical_address,
        .memory_type = Kernel::Memory::MemoryType::Normal,
        .writable = true,
        .user_allowed = false,
        .execute_disabled = false,
    };
    return powerpc_apply_kernel_mapping(table, effective_address, request);
}

extern "C" int powerpc_page_mapping_test()
{
    alignas(64 * 1024) static PowerPCHPTEntry entries[1024 * POWERPC_HPT_ENTRIES_PER_GROUP] {};
    PowerPCHashPageTable table { entries, 1024 };
    PowerPCVSIDAllocator allocator {};
    powerpc_vsid_allocator_initialize(&allocator);
    PowerPCPageDirectoryBackend directory {};
    if (!powerpc_page_directory_backend_initialize(&directory, &table, &allocator))
        return 0;

    constexpr PowerPCHPTWord address = 0x10040000u;
    Kernel::Memory::PageMappingRequest writable {
        .physical_page_base = 0x00300000u,
        .memory_type = Kernel::Memory::MemoryType::Normal,
        .writable = true,
        .user_allowed = true,
        .execute_disabled = false,
    };
    if (!powerpc_page_directory_backend_apply_mapping(&directory, address, writable))
        return 0;
    auto mapped = powerpc_page_directory_backend_query_mapping(&directory, address);
    auto* writable_entry = powerpc_hpt_find(&table, directory.user_vsid_base + 1, address);
    if (!mapped.has_value() || !mapped->present || mapped->physical_page_base != 0x00300000u
        || !mapped->writable || !mapped->user_allowed
        || !writable_entry || (writable_entry->lower & 3u) != POWERPC_HPT_READ_WRITE
        || (writable_entry->lower & 0x38u) != POWERPC_HPT_COHERENT)
        return 0;

    Kernel::Memory::PageMappingRequest read_only_io {
        .physical_page_base = 0x00400000u,
        .memory_type = Kernel::Memory::MemoryType::IO,
        .writable = false,
        .user_allowed = true,
        .execute_disabled = false,
    };
    if (!powerpc_page_directory_backend_apply_mapping(&directory, address, read_only_io))
        return 0;
    auto remapped = powerpc_page_directory_backend_query_mapping(&directory, address);
    auto* io_entry = powerpc_hpt_find(&table, directory.user_vsid_base + 1, address);
    if (!remapped.has_value() || remapped->physical_page_base != 0x00400000u
        || remapped->writable
        || !io_entry || (io_entry->lower & 3u) != POWERPC_HPT_READ_ONLY
        || (io_entry->lower & 0x38u) != (POWERPC_HPT_CACHE_INHIBITED | POWERPC_HPT_GUARDED))
        return 0;

    Kernel::Memory::PageMappingRequest noncacheable {
        .physical_page_base = 0x00500000u,
        .memory_type = Kernel::Memory::MemoryType::NonCacheable,
        .writable = true,
        .user_allowed = true,
        .execute_disabled = false,
    };
    if (!powerpc_page_directory_backend_apply_mapping(&directory, address, noncacheable))
        return 0;
    auto* noncacheable_entry = powerpc_hpt_find(&table, directory.user_vsid_base + 1, address);
    if (!noncacheable_entry || (noncacheable_entry->lower & 0x38u) != POWERPC_HPT_CACHE_INHIBITED)
        return 0;

    auto invalid = noncacheable;
    invalid.user_allowed = false;
    if (powerpc_page_directory_backend_apply_mapping(&directory, address, invalid))
        return 0;
    invalid = noncacheable;
    invalid.execute_disabled = true;
    if (powerpc_page_directory_backend_apply_mapping(&directory, address, invalid))
        return 0;
    invalid = noncacheable;
    invalid.physical_page_base = 0x1'0000'0000ull;
    if (powerpc_page_directory_backend_apply_mapping(&directory, address, invalid))
        return 0;
    auto unchanged = powerpc_page_directory_backend_query_mapping(&directory, address);
    if (!unchanged.has_value() || unchanged->physical_page_base != 0x00500000u || !unchanged->writable)
        return 0;

    if (!powerpc_page_directory_backend_unmap(&directory, address))
        return 0;
    if (powerpc_page_directory_backend_query_mapping(&directory, address).has_value())
        return 0;

    auto kernel_request = writable;
    kernel_request.user_allowed = false;
    kernel_request.writable = false;
    constexpr unsigned quickmap_address = KERNEL_MAPPING_BASE + KERNEL_PT1024_OFFSET + 3 * KERNEL_MAX_CPU_COUNT * POWERPC_HPT_PAGE_SIZE;
    if (powerpc_apply_kernel_mapping(&table, quickmap_address, kernel_request))
        return 0;
    kernel_request.writable = true;
    kernel_request.execute_disabled = true;
    if (powerpc_apply_kernel_mapping(&table, quickmap_address, kernel_request))
        return 0;

    alignas(64) PowerPCHPTEntry tiny_entries[POWERPC_HPT_ENTRIES_PER_GROUP] {};
    PowerPCHashPageTable tiny_table { tiny_entries, 1 };
    if (powerpc_map_kernel_page_array(&tiny_table, 0xc4000000u, 0x00600000u, 9))
        return 0;
    for (auto const& entry : tiny_entries) {
        if ((entry.upper & POWERPC_HPT_VALID) != 0)
            return 0;
    }
    return true;
}
