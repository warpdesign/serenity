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

static PowerPCHPTWord memory_type_flags(Kernel::Memory::MemoryType memory_type)
{
    switch (memory_type) {
    case Kernel::Memory::MemoryType::Normal:
        return POWERPC_HPT_COHERENT;
    case Kernel::Memory::MemoryType::NonCacheable:
        return POWERPC_HPT_CACHE_INHIBITED;
    case Kernel::Memory::MemoryType::IO:
        return POWERPC_HPT_CACHE_INHIBITED | POWERPC_HPT_GUARDED;
    }
    __builtin_trap();
}

bool powerpc_page_directory_backend_apply_mapping(PowerPCPageDirectoryBackend const* directory, PowerPCHPTWord effective_address, Kernel::Memory::PageMappingRequest const& request)
{
    auto vsid = powerpc_page_directory_backend_vsid_for(directory, effective_address);
    if (!vsid || !request.user_allowed || request.execute_disabled || request.physical_page_base > 0xfffff000u)
        return false;

    PowerPCHPTWord flags = POWERPC_HPT_REFERENCED | POWERPC_HPT_CHANGED | memory_type_flags(request.memory_type);
    flags |= request.writable ? POWERPC_HPT_READ_WRITE : POWERPC_HPT_READ_ONLY;

    if (powerpc_hpt_find(directory->table, vsid, effective_address))
        return powerpc_hpt_remap(directory->table, vsid, effective_address, static_cast<PowerPCHPTWord>(request.physical_page_base), flags);
    return powerpc_hpt_map(directory->table, vsid, effective_address, static_cast<PowerPCHPTWord>(request.physical_page_base), flags);
}

bool powerpc_apply_kernel_mapping(PowerPCHashPageTable const* table, PowerPCHPTWord effective_address, Kernel::Memory::PageMappingRequest const& request)
{
    if (!table || !table->entries || effective_address < 0xc0000000u
        || (effective_address & (POWERPC_HPT_PAGE_SIZE - 1)) != 0
        || request.physical_page_base > 0xfffff000u
        || (request.physical_page_base & (POWERPC_HPT_PAGE_SIZE - 1)) != 0
        || request.user_allowed || request.execute_disabled)
        return false;

    PowerPCHPTWord vsid = effective_address >> 28;
    PowerPCHPTWord flags = POWERPC_HPT_REFERENCED | POWERPC_HPT_CHANGED
        | memory_type_flags(request.memory_type)
        | (request.writable ? POWERPC_HPT_SUPERVISOR_ONLY : POWERPC_HPT_READ_ONLY);
    // PP=3 is read-only for both privilege keys. User return substitutes
    // private high VSIDs, so it cannot resolve the shared kernel entry.
    auto physical = static_cast<PowerPCHPTWord>(request.physical_page_base);
    if (powerpc_hpt_find(table, vsid, effective_address))
        return powerpc_hpt_remap(table, vsid, effective_address, physical, flags);
    return powerpc_hpt_map(table, vsid, effective_address, physical, flags);
}

int powerpc_set_kernel_page_writable(PowerPCHashPageTable const* table, PowerPCHPTWord effective_address, int writable)
{
    if (!table || effective_address < 0xc0000000u
        || (effective_address & (POWERPC_HPT_PAGE_SIZE - 1)) != 0
        || (writable != 0 && writable != 1))
        return 0;

    PowerPCHPTWord vsid = effective_address >> 28;
    auto* entry = powerpc_hpt_find(table, vsid, effective_address);
    if (!entry)
        return 0;
    PowerPCHPTWord lower = entry->lower;
    PowerPCHPTWord protection = lower & 3u;
    if (protection != POWERPC_HPT_SUPERVISOR_ONLY && protection != POWERPC_HPT_READ_ONLY)
        return 0;
    PowerPCHPTWord desired_protection = writable ? POWERPC_HPT_SUPERVISOR_ONLY : POWERPC_HPT_READ_ONLY;
    if (protection == desired_protection)
        return 1;
    return powerpc_hpt_remap(table, vsid, effective_address, lower & 0xfffff000u,
        (lower & 0xfffu & ~3u) | desired_protection);
}

int powerpc_protect_kernel_page_range(PowerPCHashPageTable const* table, PowerPCHPTWord start, PowerPCHPTWord end)
{
    if (!table || start < 0xc0000000u || end < start
        || (start & (POWERPC_HPT_PAGE_SIZE - 1)) != 0
        || (end & (POWERPC_HPT_PAGE_SIZE - 1)) != 0)
        return 0;

    // Validate the complete range before changing any entry. The caller
    // serializes the shared HPT throughout both passes.
    for (auto address = start; address < end; address += POWERPC_HPT_PAGE_SIZE) {
        auto* entry = powerpc_hpt_find(table, address >> 28, address);
        if (!entry)
            return 0;
        auto protection = entry->lower & 3u;
        if (protection != POWERPC_HPT_SUPERVISOR_ONLY && protection != POWERPC_HPT_READ_ONLY)
            return 0;
    }
    for (auto address = start; address < end; address += POWERPC_HPT_PAGE_SIZE) {
        if (!powerpc_set_kernel_page_writable(table, address, 0))
            return 0;
    }
    return 1;
}

int powerpc_unmap_kernel_page_range(PowerPCHashPageTable const* table, PowerPCHPTWord start, PowerPCHPTWord end)
{
    if (!table || start < 0xc0000000u || end < start
        || (start & (POWERPC_HPT_PAGE_SIZE - 1)) != 0
        || (end & (POWERPC_HPT_PAGE_SIZE - 1)) != 0)
        return 0;

    for (auto address = start; address < end; address += POWERPC_HPT_PAGE_SIZE) {
        if (!powerpc_hpt_find(table, address >> 28, address))
            return 0;
    }
    for (auto address = start; address < end; address += POWERPC_HPT_PAGE_SIZE) {
        if (!powerpc_hpt_unmap(table, address >> 28, address))
            return 0;
    }
    return 1;
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
