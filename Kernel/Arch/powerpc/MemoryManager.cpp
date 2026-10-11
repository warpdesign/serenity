/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Checked.h>
#include <Kernel/Arch/PageDirectory.h>
#include <Kernel/Arch/Processor.h>
#include <Kernel/Arch/powerpc/PageDirectoryBackend.h>
#include <Kernel/Arch/powerpc/PhysicalPageArray.h>
#include <Kernel/Arch/powerpc/SegmentRegisters.h>
#include <Kernel/FileSystem/OpenFileDescription.h>
#include <Kernel/Interrupts/InterruptDisabler.h>
#include <Kernel/Memory/MemoryManager.h>
#include <Kernel/Sections.h>

extern u8 start_of_kernel_image[];
extern u8 end_of_kernel_image[];

namespace Kernel::Memory {

UNMAP_AFTER_INIT void MemoryManager::initialize_physical_pages(GlobalData& global_data)
{
    PhysicalAddress highest_physical_address;
    for (auto const& range : global_data.used_memory_ranges) {
        if (range.end > highest_physical_address)
            highest_physical_address = range.end;
    }
    for (auto const& range : global_data.physical_memory_ranges) {
        auto end = range.start.offset(range.length);
        if (end > highest_physical_address)
            highest_physical_address = end;
    }
    if (!g_boot_info.boot_framebuffer.paddr.is_null() && g_boot_info.boot_framebuffer.type != BootFramebufferType::None) {
        auto end = g_boot_info.boot_framebuffer.paddr.offset(g_boot_info.boot_framebuffer.height * g_boot_info.boot_framebuffer.pitch);
        if (end > highest_physical_address)
            highest_physical_address = end;
    }

    m_physical_page_entries_count = PhysicalAddress::physical_page_index(highest_physical_address.get()) + 1;
    VERIFY(m_physical_page_entries_count != 0);
    VERIFY(!Checked<size_t>::multiplication_would_overflow(m_physical_page_entries_count, sizeof(PhysicalPageEntry)));
    auto entry_bytes = m_physical_page_entries_count * sizeof(PhysicalPageEntry);
    auto metadata_page_count = page_round_up(entry_bytes).release_value_but_fixme_should_propagate_errors() / PAGE_SIZE;
    VERIFY(metadata_page_count != 0);

    PhysicalRegion* chosen_region = nullptr;
    Optional<size_t> chosen_index;
    for (size_t index = 0; index < global_data.physical_regions.size(); ++index) {
        if (global_data.physical_regions[index]->size() >= metadata_page_count) {
            chosen_region = global_data.physical_regions[index].ptr();
            chosen_index = index;
            break;
        }
    }
    if (!chosen_region) {
        dmesgln("MM: No contiguous RAM for {} PowerPC physical-page metadata pages", metadata_page_count);
        VERIFY_NOT_REACHED();
    }

    VERIFY(global_data.system_memory_info.physical_pages >= metadata_page_count);
    global_data.system_memory_info.physical_pages -= metadata_page_count;
    if (chosen_region->size() == metadata_page_count)
        global_data.physical_pages_region = global_data.physical_regions.take(*chosen_index);
    else
        global_data.physical_pages_region = chosen_region->try_take_pages_from_beginning(metadata_page_count);
    global_data.used_memory_ranges.try_append({
        UsedMemoryRangeType::PhysicalPages,
        global_data.physical_pages_region->lower(),
        global_data.physical_pages_region->upper(),
    }).release_value_but_fixme_should_propagate_errors();

    m_kernel_page_directory = PageDirectory::must_create_kernel_page_directory();
    powerpc_activate_kernel_segments();

    auto kernel_image_start = page_round_down((FlatPtr)start_of_kernel_image);
    auto kernel_image_end = page_round_up((FlatPtr)end_of_kernel_image).release_value_but_fixme_should_propagate_errors();
    MUST(global_data.region_tree.place_specifically(*MUST(Region::create_unbacked()).leak_ptr(),
        VirtualRange { VirtualAddress(kernel_image_start), kernel_image_end - kernel_image_start }));

    auto& metadata_region = *MUST(Region::create_unbacked()).leak_ptr();
    MUST(global_data.region_tree.place_anywhere(metadata_region, RandomizeVirtualAddress::No, metadata_page_count * PAGE_SIZE));
    auto virtual_base = metadata_region.vaddr().get();
    auto physical_base = global_data.physical_pages_region->lower().get();
    VERIFY(physical_base + metadata_page_count * PAGE_SIZE <= 0x100000000ull);
    {
        SpinlockLocker locker(PageDirectory::hpt_lock());
        VERIFY(powerpc_map_kernel_page_array(m_kernel_page_directory->backend().table,
            virtual_base, static_cast<u32>(physical_base), metadata_page_count));
    }

    m_physical_page_entries = reinterpret_cast<PhysicalPageEntry*>(virtual_base);
    // Entries are raw storage until a PhysicalRAMPage is placement-new'd into
    // the allocated member. Initialize the backing bytes before that begins.
    __builtin_memset(static_cast<void*>(m_physical_page_entries), 0, metadata_page_count * PAGE_SIZE);
    dmesgln("MM: PowerPC physical-page entries: {}", metadata_region.range());
}

void MemoryManager::prepare_kernel_heap_range(VirtualRange const&)
{
    VERIFY(kernel_page_directory().get_lock().is_locked_by_current_processor());
    // The fixed hash page table needs no per-address allocation before mapping.
}

bool MemoryManager::map_page(PageDirectory& directory, VirtualAddress address, PageMappingRequest const& request)
{
    VERIFY(directory.get_lock().is_locked_by_current_processor());
    SpinlockLocker hpt_locker(PageDirectory::hpt_lock());
    if (request.user_allowed) {
        if (!directory.process())
            return false;
        return powerpc_page_directory_backend_apply_mapping(&directory.backend(), address.get(), request);
    }
    if (&directory != &kernel_page_directory())
        return false;
    return powerpc_apply_kernel_mapping(directory.backend().table, address.get(), request);
}

void MemoryManager::unmap_page(PageDirectory& directory, VirtualAddress address, bool)
{
    VERIFY(directory.get_lock().is_locked_by_current_processor());
    SpinlockLocker hpt_locker(PageDirectory::hpt_lock());
    auto page_address = address.page_base().get();
    if (page_address < KERNEL_MAPPING_BASE)
        (void)powerpc_page_directory_backend_unmap(&directory.backend(), page_address);
    else {
        VERIFY(&directory == &kernel_page_directory());
        (void)powerpc_hpt_unmap(directory.backend().table, page_address >> 28, page_address);
    }
}

Optional<PageMappingInfo> MemoryManager::query_page(PageDirectory& directory, VirtualAddress address)
{
    VERIFY(directory.get_lock().is_locked_by_current_processor());
    SpinlockLocker hpt_locker(PageDirectory::hpt_lock());
    auto page_address = address.page_base().get();
    if (page_address < KERNEL_MAPPING_BASE)
        return powerpc_page_directory_backend_query_mapping(&directory.backend(), page_address);

    auto* entry = powerpc_hpt_find(directory.backend().table, page_address >> 28, page_address);
    if (!entry)
        return {};
    u32 lower = entry->lower;
    u32 protection = lower & 3u;
    return PageMappingInfo {
        .physical_page_base = lower & 0xfffff000u,
        .present = true,
        .writable = protection != POWERPC_HPT_READ_ONLY,
        .user_allowed = protection != POWERPC_HPT_SUPERVISOR_ONLY,
        .execute_disabled = false,
    };
}

u8* MemoryManager::quickmap_page(PhysicalAddress physical_address, MemoryType memory_type)
{
    VERIFY_INTERRUPTS_DISABLED();
    auto& data = get_data();
    data.m_quickmap_previous_interrupts_state = data.m_quickmap_in_use.lock();

    VirtualAddress address(KERNEL_QUICKMAP_PER_CPU_BASE + Processor::current_id() * PAGE_SIZE);
    PageMappingRequest request {
        .physical_page_base = physical_address.get(),
        .memory_type = MemoryType::Normal,
        .writable = true,
        .user_allowed = false,
        .execute_disabled = false,
    };
    {
        SpinlockLocker hpt_locker(PageDirectory::hpt_lock());
        VERIFY(powerpc_apply_kernel_mapping(kernel_page_directory().backend().table, address.get(), request));
    }
    if (memory_type != MemoryType::Normal) {
        // quickmap_page only maps physical RAM. Clean and invalidate any old
        // cacheable copy before installing cache-inhibited attributes.
        Processor::flush_data_cache(address, PAGE_SIZE);
        request.memory_type = memory_type;
        SpinlockLocker hpt_locker(PageDirectory::hpt_lock());
        VERIFY(powerpc_apply_kernel_mapping(kernel_page_directory().backend().table, address.get(), request));
    }
    return address.as_ptr();
}

void MemoryManager::unquickmap_page()
{
    VERIFY_INTERRUPTS_DISABLED();
    auto& data = get_data();
    VERIFY(data.m_quickmap_in_use.is_locked());
    VirtualAddress address(KERNEL_QUICKMAP_PER_CPU_BASE + Processor::current_id() * PAGE_SIZE);
    {
        SpinlockLocker hpt_locker(PageDirectory::hpt_lock());
        VERIFY(powerpc_hpt_unmap(kernel_page_directory().backend().table, address.get() >> 28, address.get()));
    }
    data.m_quickmap_in_use.unlock(data.m_quickmap_previous_interrupts_state);
}

}
