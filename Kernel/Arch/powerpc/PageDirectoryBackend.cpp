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

int powerpc_page_directory_backend_unmap(PowerPCPageDirectoryBackend const* directory, PowerPCHPTWord effective_address)
{
    auto vsid = powerpc_page_directory_backend_vsid_for(directory, effective_address);
    if (!vsid)
        return 0;
    return powerpc_hpt_unmap(directory->table, vsid, effective_address);
}

int powerpc_page_directory_backend_activate(PowerPCPageDirectoryBackend const* directory)
{
    if (!directory || !directory->table)
        return 0;
    return powerpc_activate_user_segments(directory->user_vsid_base);
}
