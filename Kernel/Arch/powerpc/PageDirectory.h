/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/AtomicRefCounted.h>
#include <AK/IntrusiveRedBlackTree.h>
#include <AK/Platform.h>
#include <Kernel/Arch/powerpc/PageDirectoryBackend.h>
#include <Kernel/Arch/powerpc/SegmentRegisters.h>
#include <Kernel/Forward.h>
#include <Kernel/Locking/Spinlock.h>

VALIDATE_IS_POWERPC32()

namespace Kernel::Memory {

class PageDirectory final : public AtomicRefCounted<PageDirectory> {
    friend class MemoryManager;

public:
    static void install_shared_hpt(PowerPCHashPageTable const&);
    static ErrorOr<NonnullLockRefPtr<PageDirectory>> try_create_for_userspace(Process&);
    static NonnullLockRefPtr<PageDirectory> must_create_kernel_page_directory();
    static LockRefPtr<PageDirectory> find_current();

    ~PageDirectory();

    PowerPCPageDirectoryBackend const& backend() const { return m_backend; }
    Process* process() const { return m_process; }
    RecursiveSpinlock<LockRank::None>& get_lock() { return m_lock; }
    static Spinlock<LockRank::None>& hpt_lock();

    // The shared hardware HPT only caches mappings. These software entries
    // remain authoritative when a hash group needs to evict a user page.
    bool remember_mapping(PowerPCHPTWord effective_address, PowerPCHPTWord lower_word);
    PowerPCHPTWord shadow_mapping(PowerPCHPTWord effective_address) const;
    void forget_mapping(PowerPCHPTWord effective_address);

    // Used by the address-space registry, like the other architectures.
    IntrusiveRedBlackTreeNode<FlatPtr, PageDirectory, RawPtr<PageDirectory>> m_tree_node;

private:
    PageDirectory() = default;
    static void register_page_directory(PageDirectory&);
    static void deregister_page_directory(PageDirectory&);

    Process* m_process { nullptr };
    PowerPCPageDirectoryBackend m_backend {};
    RecursiveSpinlock<LockRank::None> m_lock {};
    static constexpr size_t shadow_table_count = POWERPC_USER_SEGMENT_COUNT * 64;
    PowerPCHPTWord* m_shadow_tables[shadow_table_count] {};
};

void activate_kernel_page_directory(PageDirectory const&);
void activate_page_directory(PageDirectory const&, Thread* current_thread);

}
