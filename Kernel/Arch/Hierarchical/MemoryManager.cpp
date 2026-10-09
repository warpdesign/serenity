/*
 * Copyright (c) 2018-2026, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Assertions.h>
#include <AK/MemoryStream.h>
#include <AK/QuickSort.h>
#include <AK/StringView.h>
#include <Kernel/Arch/CPU.h>
#include <Kernel/Arch/MemoryFences.h>
#include <Kernel/Arch/PageDirectory.h>
#include <Kernel/Arch/PageFault.h>
#include <Kernel/Arch/RegisterState.h>
#include <Kernel/Boot/BootInfo.h>
#include <Kernel/FileSystem/Inode.h>
#include <Kernel/Firmware/DeviceTree/DeviceTree.h>
#include <Kernel/Heap/kmalloc.h>
#include <Kernel/Interrupts/InterruptDisabler.h>
#include <Kernel/KSyms.h>
#include <Kernel/Library/Panic.h>
#include <Kernel/Library/StdLib.h>
#include <Kernel/Memory/AnonymousVMObject.h>
#include <Kernel/Memory/MMIOVMObject.h>
#include <Kernel/Memory/MemoryManager.h>
#include <Kernel/Memory/PhysicalRegion.h>
#include <Kernel/Memory/SharedInodeVMObject.h>
#include <Kernel/Prekernel/Prekernel.h>
#include <Kernel/Sections.h>
#include <Kernel/Security/AddressSanitizer.h>
#include <Kernel/Tasks/Process.h>
#include <Userland/Libraries/LibDeviceTree/FlattenedDeviceTree.h>

extern u8 start_of_kernel_image[];
extern u8 end_of_kernel_image[];
extern u8 start_of_kernel_text[];
extern u8 start_of_kernel_data[];
extern u8 end_of_kernel_bss[];
extern u8 start_of_ro_after_init[];
extern u8 end_of_ro_after_init[];
extern u8 start_of_unmap_after_init[];
extern u8 end_of_unmap_after_init[];
extern u8 start_of_kernel_ksyms[];
extern u8 end_of_kernel_ksyms[];

extern multiboot_module_entry_t multiboot_copy_boot_modules_array[16];
extern size_t multiboot_copy_boot_modules_count;

namespace Kernel::Memory {

UNMAP_AFTER_INIT void MemoryManager::protect_kernel_image()
{
    SpinlockLocker page_lock(kernel_page_directory().get_lock());
    // Disable writing to the kernel text and rodata segments.
    for (auto const* i = start_of_kernel_text; i < start_of_kernel_data; i += PAGE_SIZE) {
        auto& pte = *ensure_pte(kernel_page_directory(), VirtualAddress(i));
        pte.set_writable(false);
    }
    if (Processor::current().has_nx()) {
        // Disable execution of the kernel data, bss and heap segments.
        for (auto const* i = start_of_kernel_data; i < end_of_kernel_image; i += PAGE_SIZE) {
            auto& pte = *ensure_pte(kernel_page_directory(), VirtualAddress(i));
            pte.set_execute_disabled(true);
        }
    }
}

UNMAP_AFTER_INIT void MemoryManager::unmap_prekernel()
{
    SpinlockLocker page_lock(kernel_page_directory().get_lock());

    if (g_boot_info.boot_method == BootMethod::Multiboot1) {
        auto start = g_boot_info.boot_method_specific.multiboot1.start_of_prekernel_image.page_base().get();
        auto end = g_boot_info.boot_method_specific.multiboot1.end_of_prekernel_image.page_base().get();

        for (auto i = start; i <= end; i += PAGE_SIZE)
            release_pte(kernel_page_directory(), VirtualAddress(i), i == end ? IsLastPTERelease::Yes : IsLastPTERelease::No);
        flush_tlb(&kernel_page_directory(), VirtualAddress(start), (end - start) / PAGE_SIZE);
    } else if (g_boot_info.boot_method == BootMethod::EFI) {
        release_pte(kernel_page_directory(), g_boot_info.boot_method_specific.efi.bootstrap_page_vaddr, IsLastPTERelease::Yes);
        flush_tlb(&kernel_page_directory(), g_boot_info.boot_method_specific.efi.bootstrap_page_vaddr);
    }
}

UNMAP_AFTER_INIT void MemoryManager::protect_readonly_after_init_memory()
{
    SpinlockLocker page_lock(kernel_page_directory().get_lock());
    // Disable writing to the .ro_after_init section
    for (auto i = (FlatPtr)&start_of_ro_after_init; i < (FlatPtr)&end_of_ro_after_init; i += PAGE_SIZE) {
        auto& pte = *ensure_pte(kernel_page_directory(), VirtualAddress(i));
        pte.set_writable(false);
        flush_tlb(&kernel_page_directory(), VirtualAddress(i));
    }
}

void MemoryManager::unmap_text_after_init()
{
    SpinlockLocker page_lock(kernel_page_directory().get_lock());

    auto start = page_round_down((FlatPtr)&start_of_unmap_after_init);
    auto end = page_round_up((FlatPtr)&end_of_unmap_after_init).release_value_but_fixme_should_propagate_errors();

    // Unmap the entire .unmap_after_init section
    for (auto i = start; i < end; i += PAGE_SIZE) {
        auto& pte = *ensure_pte(kernel_page_directory(), VirtualAddress(i));
        pte.clear();
        flush_tlb(&kernel_page_directory(), VirtualAddress(i));
    }

    dmesgln("Unmapped {} KiB of kernel text after init! :^)", (end - start) / KiB);
}

UNMAP_AFTER_INIT void MemoryManager::protect_ksyms_after_init()
{
    SpinlockLocker page_lock(kernel_page_directory().get_lock());

    auto start = page_round_down((FlatPtr)start_of_kernel_ksyms);
    auto end = page_round_up((FlatPtr)end_of_kernel_ksyms).release_value_but_fixme_should_propagate_errors();

    for (auto i = start; i < end; i += PAGE_SIZE) {
        auto& pte = *ensure_pte(kernel_page_directory(), VirtualAddress(i));
        pte.set_writable(false);
        flush_tlb(&kernel_page_directory(), VirtualAddress(i));
    }

    dmesgln("Write-protected kernel symbols after init.");
}

UNMAP_AFTER_INIT void MemoryManager::initialize_physical_pages(GlobalData& global_data)
{
    // We assume that the physical page range is contiguous and doesn't contain huge gaps!
    PhysicalAddress highest_physical_address;
#if ARCH(AARCH64)
    // FIXME: The BCM2711/BCM2835 Raspberry Pi VideoCore region ends at 0x4000'0000.
    //        Either make MMIO usable before MM is fully initialized and use the RPi mailbox to get this address
    //        or make the physical page array dynamically resizable and possibly non-contiguos.
    highest_physical_address = PhysicalAddress { 0x4000'0000 };
#endif
    for (auto& range : global_data.used_memory_ranges) {
        if (range.end.get() > highest_physical_address.get())
            highest_physical_address = range.end;
    }
    for (auto& region : global_data.physical_memory_ranges) {
        auto range_end = PhysicalAddress(region.start).offset(region.length);
        if (range_end.get() > highest_physical_address.get())
            highest_physical_address = range_end;
    }

    if (!g_boot_info.boot_framebuffer.paddr.is_null() && g_boot_info.boot_framebuffer.type != BootFramebufferType::None) {
        PhysicalAddress boot_framebuffer_paddr_end = g_boot_info.boot_framebuffer.paddr.offset(g_boot_info.boot_framebuffer.height * g_boot_info.boot_framebuffer.pitch);
        if (boot_framebuffer_paddr_end > highest_physical_address)
            highest_physical_address = boot_framebuffer_paddr_end;
    }

    // Calculate how many total physical pages the array will have
    m_physical_page_entries_count = PhysicalAddress::physical_page_index(highest_physical_address.get()) + 1;
    VERIFY(m_physical_page_entries_count != 0);
    VERIFY(!Checked<decltype(m_physical_page_entries_count)>::multiplication_would_overflow(m_physical_page_entries_count, sizeof(PhysicalPageEntry)));

    // Calculate how many bytes the array will consume
    auto physical_page_array_size = m_physical_page_entries_count * sizeof(PhysicalPageEntry);
    auto physical_page_array_pages = page_round_up(physical_page_array_size).release_value_but_fixme_should_propagate_errors() / PAGE_SIZE;
    VERIFY(physical_page_array_pages * PAGE_SIZE >= physical_page_array_size);

    // Calculate how many page tables we will need to be able to map them all
    auto needed_page_table_count = (physical_page_array_pages + 512 - 1) / 512;

    auto physical_page_array_pages_and_page_tables_count = physical_page_array_pages + needed_page_table_count;

    // Now that we know how much memory we need for a contiguous array of PhysicalPage instances, find a memory region that can fit it
    PhysicalRegion* found_region { nullptr };
    Optional<size_t> found_region_index;
    for (size_t i = 0; i < global_data.physical_regions.size(); ++i) {
        auto& region = global_data.physical_regions[i];
        if (region->size() >= physical_page_array_pages_and_page_tables_count) {
            found_region = region;
            found_region_index = i;
            break;
        }
    }

    if (!found_region) {
        dmesgln("MM: Need {} bytes for physical page management, but no memory region is large enough!", physical_page_array_pages_and_page_tables_count);
        VERIFY_NOT_REACHED();
    }

    VERIFY(global_data.system_memory_info.physical_pages >= physical_page_array_pages_and_page_tables_count);
    global_data.system_memory_info.physical_pages -= physical_page_array_pages_and_page_tables_count;

    if (found_region->size() == physical_page_array_pages_and_page_tables_count) {
        // We're stealing the entire region
        global_data.physical_pages_region = global_data.physical_regions.take(*found_region_index);
    } else {
        global_data.physical_pages_region = found_region->try_take_pages_from_beginning(physical_page_array_pages_and_page_tables_count);
    }
    global_data.used_memory_ranges.try_append({ UsedMemoryRangeType::PhysicalPages, global_data.physical_pages_region->lower(), global_data.physical_pages_region->upper() }).release_value_but_fixme_should_propagate_errors();

    // Create the bare page directory. This is not a fully constructed page directory and merely contains the allocators!
    m_kernel_page_directory = PageDirectory::must_create_kernel_page_directory();

    {
        // Carve out the whole page directory covering the kernel image to make MemoryManager::initialize_physical_pages() happy
        FlatPtr start_of_range = ((FlatPtr)start_of_kernel_image & ~(FlatPtr)0x1fffff);
        FlatPtr end_of_range = ((FlatPtr)end_of_kernel_image & ~(FlatPtr)0x1fffff) + 0x200000;
        MUST(global_data.region_tree.place_specifically(*MUST(Region::create_unbacked()).leak_ptr(), VirtualRange { VirtualAddress(start_of_range), end_of_range - start_of_range }));
    }

    // Allocate a virtual address range for our array
    // This looks awkward, but it basically creates a dummy region to occupy the address range permanently.
    auto& region = *MUST(Region::create_unbacked()).leak_ptr();
    MUST(global_data.region_tree.place_anywhere(region, RandomizeVirtualAddress::No, physical_page_array_pages * PAGE_SIZE));
    auto range = region.range();

    // Now that we have our special m_physical_pages_region region with enough pages to hold the entire array
    // try to map the entire region into kernel space so we always have it
    // We can't use ensure_pte here because it would try to allocate a PhysicalPage and we don't have the array
    // mapped yet so we can't create them

    // Create page tables at the beginning of m_physical_pages_region, followed by the PhysicalPageEntry array
    auto page_tables_base = global_data.physical_pages_region->lower();
    auto physical_page_array_base = page_tables_base.offset(needed_page_table_count * PAGE_SIZE);
    auto physical_page_array_current_page = physical_page_array_base.get();
    auto virtual_page_array_base = range.base().get();
    auto virtual_page_array_current_page = virtual_page_array_base;
    for (size_t pt_index = 0; pt_index < needed_page_table_count; pt_index++) {
        auto virtual_page_base_for_this_pt = virtual_page_array_current_page;
        auto pt_paddr = page_tables_base.offset(pt_index * PAGE_SIZE);
        auto* pt = reinterpret_cast<PageTableEntry*>(quickmap_page(pt_paddr));
        __builtin_memset(pt, 0, PAGE_SIZE);
        for (size_t pte_index = 0; pte_index < PAGE_SIZE / sizeof(PageTableEntry); pte_index++) {
            auto& pte = pt[pte_index];
            pte.set_physical_page_base(physical_page_array_current_page);
            pte.set_user_allowed(false);
            pte.set_writable(true);
            if (Processor::current().has_nx())
                pte.set_execute_disabled(false);
            pte.set_global(true);
            pte.set_present(true);

            physical_page_array_current_page += PAGE_SIZE;
            virtual_page_array_current_page += PAGE_SIZE;
        }
        unquickmap_page();

        // Hook the page table into the kernel page directory
        u32 page_directory_index = (virtual_page_base_for_this_pt >> 21) & 0x1ff;
        auto* pd = reinterpret_cast<PageDirectoryEntry*>(quickmap_page(g_boot_info.boot_pd_kernel));
        PageDirectoryEntry& pde = pd[page_directory_index];

        VERIFY(!pde.is_present()); // Nothing should be using this PD yet

        // We can't use ensure_pte quite yet!
        pde.set_page_table_base(pt_paddr.get());
        pde.set_user_allowed(false);
        pde.set_present(true);
        pde.set_writable(true);
        pde.set_global(true);

        unquickmap_page();

        flush_tlb_local(VirtualAddress(virtual_page_base_for_this_pt));
    }

    // We now have the entire PhysicalPageEntry array mapped!
    m_physical_page_entries = (PhysicalPageEntry*)range.base().get();
    for (size_t i = 0; i < m_physical_page_entries_count; i++)
        new (&m_physical_page_entries[i]) PageTableEntry();

    // Now we should be able to allocate PhysicalPage instances,
    // so finish setting up the kernel page directory
    m_kernel_page_directory->allocate_kernel_directory();

    // Now create legit PhysicalPage objects for the page tables we created.
    virtual_page_array_current_page = virtual_page_array_base;
    for (size_t pt_index = 0; pt_index < needed_page_table_count; pt_index++) {
        VERIFY(virtual_page_array_current_page <= range.end().get());
        auto pt_paddr = page_tables_base.offset(pt_index * PAGE_SIZE);
        auto physical_page_index = PhysicalAddress::physical_page_index(pt_paddr.get());
        auto& physical_page_entry = m_physical_page_entries[physical_page_index];
        auto physical_page = adopt_lock_ref(*new (&physical_page_entry.allocated.physical_page) PhysicalRAMPage(MayReturnToFreeList::No));

        // NOTE: This leaked ref is matched by the unref in MemoryManager::release_pte()
        (void)physical_page.leak_ref();

        virtual_page_array_current_page += (PAGE_SIZE / sizeof(PageTableEntry)) * PAGE_SIZE;
    }

    dmesgln("MM: Physical page entries: {}", range);
}

PageTableEntry* MemoryManager::pte(PageDirectory& page_directory, VirtualAddress vaddr)
{
    VERIFY_INTERRUPTS_DISABLED();
    VERIFY(page_directory.get_lock().is_locked_by_current_processor());
    u32 page_directory_table_index = (vaddr.get() >> 30) & 0x1ff;
    u32 page_directory_index = (vaddr.get() >> 21) & 0x1ff;
    u32 page_table_index = (vaddr.get() >> 12) & 0x1ff;

    auto* pd = quickmap_pd(const_cast<PageDirectory&>(page_directory), page_directory_table_index);
    PageDirectoryEntry const& pde = pd[page_directory_index];
    if (!pde.is_present())
        return nullptr;

    return &quickmap_pt(PhysicalAddress((FlatPtr)pde.page_table_base()))[page_table_index];
}

PageTableEntry* MemoryManager::ensure_pte(PageDirectory& page_directory, VirtualAddress vaddr)
{
    VERIFY_INTERRUPTS_DISABLED();
    VERIFY(page_directory.get_lock().is_locked_by_current_processor());
    u32 page_directory_table_index = (vaddr.get() >> 30) & 0x1ff;
    u32 page_directory_index = (vaddr.get() >> 21) & 0x1ff;
    u32 page_table_index = (vaddr.get() >> 12) & 0x1ff;

    auto* pd = quickmap_pd(page_directory, page_directory_table_index);
    auto& pde = pd[page_directory_index];
    if (pde.is_present())
        return &quickmap_pt(PhysicalAddress(pde.page_table_base()))[page_table_index];

    bool did_purge = false;
    auto page_table_or_error = allocate_physical_page(ShouldZeroFill::Yes, &did_purge);
    if (page_table_or_error.is_error()) {
        dbgln("MM: Unable to allocate page table to map {}", vaddr);
        return nullptr;
    }
    auto page_table = page_table_or_error.release_value();
    if (did_purge) {
        // If any memory had to be purged, ensure_pte may have been called as part
        // of the purging process. So we need to re-map the pd in this case to ensure
        // we're writing to the correct underlying physical page
        pd = quickmap_pd(page_directory, page_directory_table_index);
        VERIFY(&pde == &pd[page_directory_index]); // Sanity check

        VERIFY(!pde.is_present()); // Should have not changed
    }
    pde.set_page_table_base(page_table->paddr().get());
    pde.set_user_allowed(true);
    pde.set_present(true);
    pde.set_writable(true);
    pde.set_global(&page_directory == m_kernel_page_directory.ptr());

    // NOTE: This leaked ref is matched by the unref in MemoryManager::release_pte()
    (void)page_table.leak_ref();

    return &quickmap_pt(PhysicalAddress(pde.page_table_base()))[page_table_index];
}

void MemoryManager::release_pte(PageDirectory& page_directory, VirtualAddress vaddr, IsLastPTERelease is_last_pte_release)
{
    VERIFY_INTERRUPTS_DISABLED();
    VERIFY(page_directory.get_lock().is_locked_by_current_processor());
    u32 page_directory_table_index = (vaddr.get() >> 30) & 0x1ff;
    u32 page_directory_index = (vaddr.get() >> 21) & 0x1ff;
    u32 page_table_index = (vaddr.get() >> 12) & 0x1ff;

    auto* pd = quickmap_pd(page_directory, page_directory_table_index);
    PageDirectoryEntry& pde = pd[page_directory_index];
    if (pde.is_present()) {
        auto* page_table = quickmap_pt(PhysicalAddress((FlatPtr)pde.page_table_base()));
        auto& pte = page_table[page_table_index];
        pte.clear();

        if (is_last_pte_release == IsLastPTERelease::Yes || page_table_index == 0x1ff) {
            // If this is the last PTE in a region or the last PTE in a page table then
            // check if we can also release the page table
            bool all_clear = true;
            for (u32 i = 0; i <= 0x1ff; i++) {
                if (!page_table[i].is_null()) {
                    all_clear = false;
                    break;
                }
            }
            if (all_clear) {
                get_physical_page_entry(PhysicalAddress { pde.page_table_base() }).allocated.physical_page.unref();
                pde.clear();
            }
        }
    }
}

PageDirectoryEntry* MemoryManager::quickmap_pd(PageDirectory& directory, size_t pdpt_index)
{
    VERIFY_INTERRUPTS_DISABLED();

    VirtualAddress vaddr(KERNEL_QUICKMAP_PD_PER_CPU_BASE + Processor::current_id() * PAGE_SIZE);
    size_t pte_index = (vaddr.get() - KERNEL_PT1024_BASE) / PAGE_SIZE;

    auto& pte = g_boot_info.boot_pd_kernel_pt1023[pte_index];
    auto pd_paddr = directory.m_directory_pages[pdpt_index]->paddr();
    if (pte.physical_page_base() != pd_paddr.get()) {
        pte.set_physical_page_base(pd_paddr.get());
        pte.set_present(true);
        pte.set_writable(true);
        pte.set_user_allowed(false);
        flush_tlb_local(vaddr);
    }
    return (PageDirectoryEntry*)vaddr.get();
}

PageTableEntry* MemoryManager::quickmap_pt(PhysicalAddress pt_paddr)
{
    VERIFY_INTERRUPTS_DISABLED();

    VirtualAddress vaddr(KERNEL_QUICKMAP_PT_PER_CPU_BASE + Processor::current_id() * PAGE_SIZE);
    size_t pte_index = (vaddr.get() - KERNEL_PT1024_BASE) / PAGE_SIZE;

    auto& pte = g_boot_info.boot_pd_kernel_pt1023[pte_index];
    if (pte.physical_page_base() != pt_paddr.get()) {
        pte.set_physical_page_base(pt_paddr.get());
        pte.set_present(true);
        pte.set_writable(true);
        pte.set_user_allowed(false);
        flush_tlb_local(vaddr);
    }
    return (PageTableEntry*)vaddr.get();
}

u8* MemoryManager::quickmap_page(PhysicalAddress physical_address, MemoryType memory_type)
{
    VERIFY_INTERRUPTS_DISABLED();
    auto& mm_data = get_data();
    mm_data.m_quickmap_previous_interrupts_state = mm_data.m_quickmap_in_use.lock();

    VirtualAddress vaddr(KERNEL_QUICKMAP_PER_CPU_BASE + Processor::current_id() * PAGE_SIZE);
    u32 pte_idx = (vaddr.get() - KERNEL_PT1024_BASE) / PAGE_SIZE;

    auto& pte = g_boot_info.boot_pd_kernel_pt1023[pte_idx];
    if (pte.physical_page_base() != physical_address.get()) {
        pte.set_memory_type(memory_type);
        pte.set_physical_page_base(physical_address.get());
        pte.set_present(true);
        pte.set_writable(true);
        pte.set_user_allowed(false);
        flush_tlb_local(vaddr);
    }
    return vaddr.as_ptr();
}

void MemoryManager::unquickmap_page()
{
    VERIFY_INTERRUPTS_DISABLED();
    auto& mm_data = get_data();
    VERIFY(mm_data.m_quickmap_in_use.is_locked());
    VirtualAddress vaddr(KERNEL_QUICKMAP_PER_CPU_BASE + Processor::current_id() * PAGE_SIZE);
    u32 pte_idx = (vaddr.get() - KERNEL_PT1024_BASE) / PAGE_SIZE;
    auto& pte = g_boot_info.boot_pd_kernel_pt1023[pte_idx];
    pte.clear();
    flush_tlb_local(vaddr);
    mm_data.m_quickmap_in_use.unlock(mm_data.m_quickmap_previous_interrupts_state);
}

void MemoryManager::set_page_writable_direct(VirtualAddress vaddr, bool writable)
{
    SpinlockLocker page_lock(kernel_page_directory().get_lock());
    auto* pte = ensure_pte(kernel_page_directory(), vaddr);
    VERIFY(pte);
    if (pte->is_writable() == writable)
        return;
    pte->set_writable(writable);
    flush_tlb(&kernel_page_directory(), vaddr);
}

}
