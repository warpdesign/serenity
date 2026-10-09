/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/PageDirectoryBackend.h>
#include <Kernel/Arch/powerpc/SegmentRegisters.h>

int powerpc_page_directory_backend_initialize(PowerPCPageDirectoryBackend* directory, PowerPCHashPageTable const* table, PowerPCVSIDAllocator* allocator)
{
    if (!directory || !table || !table->entries || table->group_count == 0
        || (table->group_count & (table->group_count - 1)) != 0)
        return 0;

    auto base = powerpc_vsid_allocator_allocate(allocator);
    if (!powerpc_user_vsid_base_valid(base))
        return 0;

    directory->table = table;
    directory->user_vsid_base = base;
    return 1;
}

PowerPCVSIDWord powerpc_page_directory_backend_vsid_for(PowerPCPageDirectoryBackend const* directory, PowerPCHPTWord effective_address)
{
    if (!directory || !directory->table || !powerpc_user_vsid_base_valid(directory->user_vsid_base))
        return 0;

    auto segment = effective_address >> 28;
    if (segment >= POWERPC_USER_SEGMENT_COUNT)
        return 0;
    return directory->user_vsid_base + segment;
}

int powerpc_page_directory_backend_map(PowerPCPageDirectoryBackend const* directory, PowerPCHPTWord effective_address, PowerPCHPTWord physical_address, PowerPCHPTWord lower_flags)
{
    auto vsid = powerpc_page_directory_backend_vsid_for(directory, effective_address);
    if (!vsid)
        return 0;
    return powerpc_hpt_map(directory->table, vsid, effective_address, physical_address, lower_flags);
}

int powerpc_page_directory_backend_remap(PowerPCPageDirectoryBackend const* directory, PowerPCHPTWord effective_address, PowerPCHPTWord physical_address, PowerPCHPTWord lower_flags)
{
    auto vsid = powerpc_page_directory_backend_vsid_for(directory, effective_address);
    if (!vsid)
        return 0;
    return powerpc_hpt_remap(directory->table, vsid, effective_address, physical_address, lower_flags);
}

int powerpc_page_directory_backend_unmap(PowerPCPageDirectoryBackend const* directory, PowerPCHPTWord effective_address)
{
    auto vsid = powerpc_page_directory_backend_vsid_for(directory, effective_address);
    if (!vsid)
        return 0;
    return powerpc_hpt_unmap(directory->table, vsid, effective_address);
}

int powerpc_page_directory_backend_purge(PowerPCPageDirectoryBackend const* directory)
{
    if (!directory || !directory->table || !powerpc_user_vsid_base_valid(directory->user_vsid_base))
        return -1;
    return powerpc_hpt_purge_vsid_block(directory->table, directory->user_vsid_base, POWERPC_USER_SEGMENT_COUNT);
}

int powerpc_page_directory_backend_activate(PowerPCPageDirectoryBackend const* directory)
{
    if (!directory || !directory->table)
        return 0;
    return powerpc_activate_user_segments(directory->user_vsid_base);
}

bool powerpc_page_directory_backend_apply_mapping(PowerPCPageDirectoryBackend const* directory, PowerPCHPTWord effective_address, Kernel::Memory::PageMappingRequest const& request)
{
    auto vsid = powerpc_page_directory_backend_vsid_for(directory, effective_address);
    if (!vsid || !request.user_allowed || request.execute_disabled || request.physical_page_base > 0xfffff000u)
        return false;

    PowerPCHPTWord flags = POWERPC_HPT_REFERENCED | POWERPC_HPT_CHANGED;
    flags |= request.writable ? POWERPC_HPT_READ_WRITE : POWERPC_HPT_READ_ONLY;
    switch (request.memory_type) {
    case Kernel::Memory::MemoryType::Normal:
        flags |= POWERPC_HPT_COHERENT;
        break;
    case Kernel::Memory::MemoryType::NonCacheable:
        flags |= POWERPC_HPT_CACHE_INHIBITED;
        break;
    case Kernel::Memory::MemoryType::IO:
        flags |= POWERPC_HPT_CACHE_INHIBITED | POWERPC_HPT_GUARDED;
        break;
    }

    if (powerpc_hpt_find(directory->table, vsid, effective_address))
        return powerpc_hpt_remap(directory->table, vsid, effective_address, static_cast<PowerPCHPTWord>(request.physical_page_base), flags);
    return powerpc_hpt_map(directory->table, vsid, effective_address, static_cast<PowerPCHPTWord>(request.physical_page_base), flags);
}

Optional<Kernel::Memory::PageMappingInfo> powerpc_page_directory_backend_query_mapping(PowerPCPageDirectoryBackend const* directory, PowerPCHPTWord effective_address)
{
    auto vsid = powerpc_page_directory_backend_vsid_for(directory, effective_address);
    if (!vsid)
        return {};
    auto* entry = powerpc_hpt_find(directory->table, vsid, effective_address);
    if (!entry)
        return {};

    PowerPCHPTWord lower = entry->lower;
    return Kernel::Memory::PageMappingInfo {
        .physical_page_base = lower & 0xfffff000u,
        .present = true,
        .writable = (lower & 3u) == POWERPC_HPT_READ_WRITE,
        .user_allowed = true,
        .execute_disabled = false,
    };
}
