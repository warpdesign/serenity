/*
 * Copyright (c) 2018-2026, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/PageDirectory.h>
#include <Kernel/Arch/Processor.h>
#include <Kernel/Memory/MemoryManager.h>

namespace Kernel::Memory {

bool MemoryManager::map_page(PageDirectory& directory, VirtualAddress address, PageMappingRequest const& request)
{
    auto* entry = ensure_pte(directory, address);
    if (!entry)
        return false;

    entry->set_memory_type(request.memory_type);
    entry->set_physical_page_base(request.physical_page_base);
    entry->set_present(true);
    entry->set_writable(request.writable);
    if (Processor::current().has_nx())
        entry->set_execute_disabled(request.execute_disabled);
    entry->set_user_allowed(request.user_allowed);
    return true;
}

void MemoryManager::unmap_page(PageDirectory& directory, VirtualAddress address, bool release_empty_table)
{
    release_pte(directory, address, release_empty_table ? IsLastPTERelease::Yes : IsLastPTERelease::No);
}

Optional<PageMappingInfo> MemoryManager::query_page(PageDirectory& directory, VirtualAddress address)
{
    auto* entry = pte(directory, address);
    if (!entry)
        return {};

    return PageMappingInfo {
        .physical_page_base = entry->physical_page_base(),
        .present = entry->is_present(),
        .writable = entry->is_writable(),
        .user_allowed = entry->is_user_allowed(),
        .execute_disabled = entry->is_execute_disabled(),
    };
}

}
