/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>

namespace Kernel {

// Keep each dirty block stable while it is written, then release the cache
// lock before selecting the next block. Bound this pass to the entries that
// were dirty at its start so concurrent writers cannot prolong it forever.
template<typename ProtectedCache, typename Writer, typename AfterBlock>
ErrorOr<size_t> write_back_dirty_blocks(ProtectedCache& protected_cache, Writer&& write, AfterBlock&& after_block)
{
    auto initial_dirty_count = protected_cache.with_exclusive([](auto& cache) { return cache->dirty_count(); });
    size_t written_count = 0;
    for (size_t index = 0; index < initial_dirty_count; ++index) {
        auto wrote = TRY(protected_cache.with_exclusive([&](auto& cache) -> ErrorOr<bool> {
            auto* entry = cache->first_dirty_entry();
            if (!entry)
                return false;
            TRY(write(*entry));
            cache->mark_clean(*entry);
            return true;
        }));
        if (!wrote)
            break;
        ++written_count;
        after_block();
    }
    return written_count;
}

template<typename ProtectedCache, typename BlockIndex, typename Writer>
ErrorOr<void> write_back_block_if_dirty(ProtectedCache& protected_cache, BlockIndex block_index, Writer&& write)
{
    return protected_cache.with_exclusive([&](auto& cache) -> ErrorOr<void> {
        auto* entry = cache->get(block_index);
        if (!entry || !cache->entry_is_dirty(*entry))
            return {};
        TRY(write(*entry));
        cache->mark_clean(*entry);
        return {};
    });
}

template<typename ProtectedCache, typename BlockIndex, typename FlushWriter, typename DirectWriter>
ErrorOr<void> write_direct_block(ProtectedCache& protected_cache, BlockIndex block_index, FlushWriter&& flush_dirty, DirectWriter&& write_direct)
{
    return protected_cache.with_exclusive([&](auto& cache) -> ErrorOr<void> {
        auto* entry = cache->get(block_index);
        if (entry && cache->entry_is_dirty(*entry)) {
            TRY(flush_dirty(*entry));
            cache->mark_clean(*entry);
        }
        // A later cached read must not return data from before the direct write,
        // including when the device reports a partial write or an error.
        cache->invalidate(block_index);
        return write_direct();
    });
}

}
