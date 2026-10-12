/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Checked.h>
#include <Kernel/Arch/PageDirectory.h>
#include <Kernel/Arch/Processor.h>
#include <Kernel/Arch/powerpc/HashPageFault.h>
#include <Kernel/Arch/powerpc/HashPageTable.h>
#include <Kernel/Arch/powerpc/PageDirectoryBackend.h>
#include <Kernel/Arch/powerpc/PhysicalPageArray.h>
#include <Kernel/Arch/powerpc/SegmentRegisters.h>
#include <Kernel/FileSystem/OpenFileDescription.h>
#include <Kernel/Interrupts/InterruptDisabler.h>
#include <Kernel/Memory/MemoryManager.h>
#include <Kernel/Sections.h>

extern u8 start_of_kernel_image[];
extern u8 end_of_kernel_image[];
extern u8 start_of_kernel_text[];
extern u8 start_of_kernel_data[];
extern u8 start_of_ro_after_init[];
extern u8 end_of_ro_after_init[];
extern u8 start_of_kernel_ksyms[];
extern u8 end_of_kernel_ksyms[];
extern u8 start_of_unmap_after_init[];
extern u8 end_of_unmap_after_init[];

namespace Kernel::Memory {

static VirtualRange boot_hpt_alias_range()
{
    auto const& arch = g_boot_info.arch_specific;
    return { VirtualAddress { arch.hpt_virtual_address },
        arch.hpt_group_count * POWERPC_HPT_ENTRIES_PER_GROUP * sizeof(PowerPCHPTEntry) };
}

static int may_evict_user_mapping(PowerPCHPTWord vsid, PowerPCHPTWord effective_address, void*)
{
    return vsid >= POWERPC_FIRST_USER_VSID_BASE && effective_address < KERNEL_MAPPING_BASE;
}

static bool apply_kernel_mapping_with_user_eviction(PowerPCHashPageTable const* table, PowerPCHPTWord effective_address, PageMappingRequest const& request)
{
    if (powerpc_apply_kernel_mapping(table, effective_address, request))
        return true;
    if (powerpc_hpt_find(table, effective_address >> 28, effective_address))
        return false;
    if (request.physical_page_base > 0xfffff000u || (request.physical_page_base & (PAGE_SIZE - 1)) != 0
        || request.user_allowed || request.execute_disabled)
        return false;
    return powerpc_hpt_map_with_eviction(table, effective_address >> 28, effective_address,
        static_cast<PowerPCHPTWord>(request.physical_page_base), powerpc_page_mapping_lower_flags(request, false),
        may_evict_user_mapping, nullptr);
}

UNMAP_AFTER_INIT void MemoryManager::protect_kernel_image()
{
    SpinlockLocker page_lock(kernel_page_directory().get_lock());
    SpinlockLocker hpt_lock(PageDirectory::hpt_lock());
    VERIFY(powerpc_protect_kernel_page_range(kernel_page_directory().backend().table,
        FlatPtr(start_of_kernel_text), FlatPtr(start_of_kernel_data)));
    // The 750 hashed MMU has no execute-disable bit for the writable data.
}

UNMAP_AFTER_INIT void MemoryManager::protect_readonly_after_init_memory()
{
    auto start = page_round_down(FlatPtr(start_of_ro_after_init));
    auto end = page_round_up(FlatPtr(end_of_ro_after_init)).release_value_but_fixme_should_propagate_errors();
    SpinlockLocker page_lock(kernel_page_directory().get_lock());
    SpinlockLocker hpt_lock(PageDirectory::hpt_lock());
    VERIFY(powerpc_protect_kernel_page_range(kernel_page_directory().backend().table, start, end));
}

UNMAP_AFTER_INIT void MemoryManager::protect_ksyms_after_init()
{
    auto start = page_round_down(FlatPtr(start_of_kernel_ksyms));
    auto end = page_round_up(FlatPtr(end_of_kernel_ksyms)).release_value_but_fixme_should_propagate_errors();
    SpinlockLocker page_lock(kernel_page_directory().get_lock());
    SpinlockLocker hpt_lock(PageDirectory::hpt_lock());
    VERIFY(powerpc_protect_kernel_page_range(kernel_page_directory().backend().table, start, end));
    dmesgln("Write-protected kernel symbols after init.");
}

UNMAP_AFTER_INIT void MemoryManager::unmap_prekernel()
{
    // FirmwareMemoryMap boots directly into the kernel image. Its physical
    // vector pages stay reserved and there is no mapped prekernel image.
    VERIFY(g_boot_info.boot_method == BootMethod::FirmwareMemoryMap);
}

void MemoryManager::unmap_text_after_init()
{
    auto start = page_round_down(FlatPtr(start_of_unmap_after_init));
    auto end = page_round_up(FlatPtr(end_of_unmap_after_init)).release_value_but_fixme_should_propagate_errors();
    SpinlockLocker page_lock(kernel_page_directory().get_lock());
    SpinlockLocker hpt_lock(PageDirectory::hpt_lock());
    VERIFY(powerpc_unmap_kernel_page_range(kernel_page_directory().backend().table, start, end));
    dmesgln("Unmapped {} KiB of kernel text after init! :^)", (end - start) / KiB);
}

UNMAP_AFTER_INIT void MemoryManager::initialize_physical_pages(GlobalData& global_data)
{
    // PhysicalRAMPage also represents PCI framebuffer pages that are not in
    // firmware's RAM map. A flat array covering the 32-bit physical bus keeps
    // those pages addressable when their BARs are assigned after MM startup.
    m_physical_page_entries_count = (1ull << 32) / PAGE_SIZE;
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

    auto& hpt_alias_region = *MUST(Region::create_unbacked()).leak_ptr();
    MUST(global_data.region_tree.place_specifically(hpt_alias_region, boot_hpt_alias_range()));

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

void MemoryManager::prepare_kernel_heap_range(VirtualRange const& range)
{
    VERIFY(kernel_page_directory().get_lock().is_locked_by_current_processor());
    auto hpt_alias = boot_hpt_alias_range();
    VERIFY(!range.intersects(hpt_alias));
    dmesgln("MM: PowerPC HPT alias: {}", hpt_alias);
    dmesgln("MM: PowerPC kmalloc expansion range: {}", range);
    // Kernel mappings stay pinned in the shared HPT. Only user mappings are
    // backed by software page tables and eligible for eviction.
}

bool MemoryManager::map_page(PageDirectory& directory, VirtualAddress address, PageMappingRequest const& request)
{
    VERIFY(directory.get_lock().is_locked_by_current_processor());
    auto page_address = address.page_base().get();
    if (request.physical_page_base > 0xfffff000u || (request.physical_page_base & (PAGE_SIZE - 1)) != 0
        || request.execute_disabled)
        return false;
    if (request.user_allowed) {
        if (!directory.process())
            return false;
        auto vsid = powerpc_page_directory_backend_vsid_for(&directory.backend(), page_address);
        if (!vsid)
            return false;
        auto previous = directory.shadow_mapping(page_address);
        auto lower = static_cast<PowerPCHPTWord>(request.physical_page_base) | powerpc_page_mapping_lower_flags(request, true);
        if (!directory.remember_mapping(page_address, lower))
            return false;
        if (!previous)
            return true;
        SpinlockLocker hpt_locker(PageDirectory::hpt_lock());
        auto* entry = powerpc_hpt_find(directory.backend().table, vsid, page_address);
        if (entry && !powerpc_hpt_remap(directory.backend().table, vsid, page_address,
                         static_cast<PowerPCHPTWord>(request.physical_page_base), lower & 0xfffu)) {
            if (previous)
                VERIFY(directory.remember_mapping(page_address, previous));
            else
                directory.forget_mapping(page_address);
            return false;
        }
        return true;
    }
    if (&directory != &kernel_page_directory())
        return false;
    SpinlockLocker hpt_locker(PageDirectory::hpt_lock());
    return apply_kernel_mapping_with_user_eviction(directory.backend().table, page_address, request);
}

void MemoryManager::unmap_page(PageDirectory& directory, VirtualAddress address, bool)
{
    VERIFY(directory.get_lock().is_locked_by_current_processor());
    SpinlockLocker hpt_locker(PageDirectory::hpt_lock());
    auto page_address = address.page_base().get();
    if (page_address < KERNEL_MAPPING_BASE) {
        directory.forget_mapping(page_address);
        (void)powerpc_page_directory_backend_unmap(&directory.backend(), page_address);
    } else {
        VERIFY(&directory == &kernel_page_directory());
        (void)powerpc_hpt_unmap(directory.backend().table, page_address >> 28, page_address);
    }
}

Optional<PageMappingInfo> MemoryManager::query_page(PageDirectory& directory, VirtualAddress address)
{
    VERIFY(directory.get_lock().is_locked_by_current_processor());
    auto page_address = address.page_base().get();
    if (page_address < KERNEL_MAPPING_BASE) {
        auto lower = directory.shadow_mapping(page_address);
        if (!lower)
            return {};
        return PageMappingInfo {
            .physical_page_base = lower & 0xfffff000u,
            .present = true,
            .writable = (lower & 3u) == POWERPC_HPT_READ_WRITE,
            .user_allowed = true,
            .execute_disabled = false,
        };
    }

    SpinlockLocker hpt_locker(PageDirectory::hpt_lock());
    auto* entry = powerpc_hpt_find(directory.backend().table, page_address >> 28, page_address);
    if (!entry)
        return {};
    u32 lower = entry->lower;
    u32 protection = lower & 3u;
    return PageMappingInfo {
        .physical_page_base = lower & 0xfffff000u,
        .present = true,
        .writable = protection == POWERPC_HPT_SUPERVISOR_ONLY,
        .user_allowed = false,
        .execute_disabled = false,
    };
}

bool try_refill_powerpc_hash_page(VirtualAddress address)
{
    auto page_address = address.page_base().get();
    if (page_address >= KERNEL_MAPPING_BASE)
        return false;

    auto directory = PageDirectory::find_current();
    if (!directory)
        return false;
    SpinlockLocker directory_locker(directory->get_lock());
    auto lower = directory->shadow_mapping(page_address);
    if (!lower)
        return false;
    auto vsid = powerpc_page_directory_backend_vsid_for(&directory->backend(), page_address);
    if (!vsid)
        return false;

    SpinlockLocker hpt_locker(PageDirectory::hpt_lock());
    if (powerpc_hpt_find(directory->backend().table, vsid, page_address))
        return false;
    return powerpc_hpt_map_with_eviction(directory->backend().table, vsid, page_address,
        lower & 0xfffff000u, lower & 0xfffu, may_evict_user_mapping, nullptr);
}

void MemoryManager::set_page_writable_direct(VirtualAddress address, bool writable)
{
    VERIFY(address.get() >= KERNEL_MAPPING_BASE);
    SpinlockLocker page_lock(kernel_page_directory().get_lock());
    SpinlockLocker hpt_lock(PageDirectory::hpt_lock());
    VERIFY(powerpc_set_kernel_page_writable(kernel_page_directory().backend().table,
        address.page_base().get(), writable));
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
        VERIFY(apply_kernel_mapping_with_user_eviction(kernel_page_directory().backend().table, address.get(), request));
    }
    if (memory_type != MemoryType::Normal) {
        // quickmap_page only maps physical RAM. Clean and invalidate any old
        // cacheable copy before installing cache-inhibited attributes.
        Processor::flush_data_cache(address, PAGE_SIZE);
        request.memory_type = memory_type;
        SpinlockLocker hpt_locker(PageDirectory::hpt_lock());
        VERIFY(apply_kernel_mapping_with_user_eviction(kernel_page_directory().backend().table, address.get(), request));
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
