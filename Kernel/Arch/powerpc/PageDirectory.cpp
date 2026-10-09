/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Singleton.h>
#include <Kernel/API/POSIX/errno.h>
#include <Kernel/Arch/PageDirectory.h>
#include <Kernel/Arch/powerpc/BootHandoff.h>
#include <Kernel/Arch/powerpc/SegmentRegisters.h>
#include <Kernel/Boot/BootInfo.h>
#include <Kernel/Library/LockRefPtr.h>

namespace Kernel::Memory {

struct VSIDDirectoryMap {
    RecursiveSpinlockProtected<IntrusiveRedBlackTree<&PageDirectory::m_tree_node>, LockRank::None> map {};
};

static Singleton<VSIDDirectoryMap> s_directory_map;
static PowerPCHashPageTable const* s_shared_hpt;
static PowerPCHashPageTable s_boot_hpt;
static PowerPCVSIDAllocator s_vsid_allocator;
static Spinlock<LockRank::None> s_hpt_lock;

Spinlock<LockRank::None>& PageDirectory::hpt_lock()
{
    return s_hpt_lock;
}

void PageDirectory::install_shared_hpt(PowerPCHashPageTable const& table)
{
    VERIFY(s_shared_hpt == nullptr);
    VERIFY(table.entries != nullptr);
    VERIFY(table.group_count != 0 && (table.group_count & (table.group_count - 1)) == 0);
    s_shared_hpt = &table;
    powerpc_vsid_allocator_initialize(&s_vsid_allocator);
}

void PageDirectory::register_page_directory(PageDirectory& directory)
{
    s_directory_map->map.with([&](auto& map) {
        map.insert(directory.m_backend.user_vsid_base, directory);
    });
}

void PageDirectory::deregister_page_directory(PageDirectory& directory)
{
    s_directory_map->map.with([&](auto& map) {
        map.remove(directory.m_backend.user_vsid_base);
    });
}

ErrorOr<NonnullLockRefPtr<PageDirectory>> PageDirectory::try_create_for_userspace(Process& process)
{
    VERIFY(s_shared_hpt != nullptr);
    auto directory = TRY(adopt_nonnull_lock_ref_or_enomem(new (nothrow) PageDirectory));
    if (!powerpc_page_directory_backend_initialize(&directory->m_backend, s_shared_hpt, &s_vsid_allocator))
        return ENOMEM;
    directory->m_process = &process;
    register_page_directory(*directory);
    return directory;
}

NonnullLockRefPtr<PageDirectory> PageDirectory::must_create_kernel_page_directory()
{
    if (!s_shared_hpt) {
        VERIFY(PowerPC::boot_hpt_from_boot_info(g_boot_info, s_boot_hpt));
        install_shared_hpt(s_boot_hpt);
    }
    auto directory = adopt_lock_ref_if_nonnull(new (nothrow) PageDirectory).release_nonnull();
    directory->m_backend.table = s_shared_hpt;
    return directory;
}

LockRefPtr<PageDirectory> PageDirectory::find_current()
{
    auto vsid = powerpc_read_segment_register(1) & 0x00ffffffu;
    if (vsid == 0)
        return {};
    auto base = vsid - 1;
    if (!powerpc_user_vsid_base_valid(base))
        return {};
    return s_directory_map->map.with([&](auto& map) {
        return map.find(base);
    });
}

PageDirectory::~PageDirectory()
{
    if (!m_process)
        return;
    deregister_page_directory(*this);
    SpinlockLocker locker(hpt_lock());
    VERIFY(powerpc_page_directory_backend_purge(&m_backend) >= 0);
}

void activate_kernel_page_directory(PageDirectory const& directory)
{
    VERIFY(directory.process() == nullptr);
    powerpc_activate_kernel_segments();
}

}
