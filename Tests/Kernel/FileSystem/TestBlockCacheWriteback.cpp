/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/FileSystem/BlockCacheWriteback.h>
#include <cerrno>
#include <cstdlib>
#include <functional>
#include <memory>
#include <vector>

extern "C" void ak_verification_failed(char const*)
{
    std::abort();
}

struct Entry {
    int block;
    bool dirty { true };
    bool has_data { true };
    int data { 0 };
};

struct Cache {
    std::vector<Entry> entries;

    size_t dirty_count() const
    {
        size_t count = 0;
        for (auto const& entry : entries)
            count += entry.dirty;
        return count;
    }

    Entry* first_dirty_entry()
    {
        for (auto& entry : entries) {
            if (entry.dirty)
                return &entry;
        }
        return nullptr;
    }

    Entry* get(int block)
    {
        for (auto& entry : entries) {
            if (entry.block == block)
                return &entry;
        }
        return nullptr;
    }

    bool entry_is_dirty(Entry const& entry) const { return entry.dirty; }
    void mark_clean(Entry& entry) { entry.dirty = false; }
    void invalidate(int block)
    {
        if (auto* entry = get(block))
            entry->has_data = false;
    }
};

struct ProtectedCache {
    std::unique_ptr<Cache> cache;
    bool locked { false };
    std::function<void(Cache&)> after_unlock;

    template<typename Callback>
    auto with_exclusive(Callback callback)
    {
        if (locked)
            __builtin_trap();
        locked = true;
        auto result = callback(cache);
        locked = false;
        if (after_unlock)
            after_unlock(*cache);
        return result;
    }
};

int main(int argc, char**)
{
    ProtectedCache cache;
    cache.cache = std::make_unique<Cache>(Cache { .entries = { { 1 }, { 2 }, { 3 } } });
    bool interleaved_reader_ran = false;
    cache.after_unlock = [&](Cache& entries) {
        if (!interleaved_reader_ran && entries.dirty_count() == 2) {
            // A reader can enter the cache after block 1, while the writeback
            // still has blocks 2 and 3 to write.
            interleaved_reader_ran = true;
            entries.entries.push_back({ 4 });
        }
    };
    std::vector<int> written;
    size_t times_released_for_reader = 0;
    auto result = Kernel::write_back_dirty_blocks(cache, [&](Entry& entry) -> ErrorOr<void> {
        if (!cache.locked)
            return Error::from_errno(EIO);
        written.push_back(entry.block);
        return {};
    }, [&] {
        if (!cache.locked)
            ++times_released_for_reader;
    });
    if (result.is_error() || result.value() != 3 || !interleaved_reader_ran || times_released_for_reader != 3)
        return 1;
    if (written != std::vector<int> { 1, 2, 3 } || cache.cache->dirty_count() != 1 || !cache.cache->entries.back().dirty)
        return 1;

    if (argc > 1) {
        ProtectedCache failing;
        failing.cache = std::make_unique<Cache>(Cache { .entries = { { 1 }, { 2 }, { 3 } } });
        auto failed_result = Kernel::write_back_dirty_blocks(failing, [&](Entry& entry) -> ErrorOr<void> {
            if (entry.block == 2)
                return Error::from_errno(EIO);
            return {};
        }, [] {});
        if (!failed_result.is_error() || failing.cache->entries[0].dirty || !failing.cache->entries[1].dirty || !failing.cache->entries[2].dirty)
            return 1;

        ProtectedCache direct;
        direct.cache = std::make_unique<Cache>(Cache { .entries = { { 7 } } });
        direct.cache->entries[0].data = 11;
        int disk_value = 0;
        auto direct_result = Kernel::write_direct_block(direct, 7,
            [&](Entry& entry) -> ErrorOr<void> {
                disk_value = entry.data;
                return {};
            },
            [&]() -> ErrorOr<void> {
                if (disk_value != 11)
                    return Error::from_errno(EIO);
                disk_value = 22;
                return {};
            });
        if (direct_result.is_error() || disk_value != 22 || direct.cache->entries[0].dirty || direct.cache->entries[0].has_data)
            return 1;

        ProtectedCache failed_flush;
        failed_flush.cache = std::make_unique<Cache>(Cache { .entries = { { 8 } } });
        bool direct_write_ran = false;
        auto result_after_failed_flush = Kernel::write_direct_block(failed_flush, 8,
            [&](Entry&) -> ErrorOr<void> { return Error::from_errno(EIO); },
            [&]() -> ErrorOr<void> {
                direct_write_ran = true;
                return {};
            });
        if (!result_after_failed_flush.is_error() || direct_write_ran || !failed_flush.cache->entries[0].dirty)
            return 1;

        ProtectedCache direct_read;
        direct_read.cache = std::make_unique<Cache>(Cache { .entries = { { 9 } } });
        size_t flush_count = 0;
        auto flush_once = [&](Entry&) -> ErrorOr<void> {
            ++flush_count;
            return {};
        };
        if (Kernel::write_back_block_if_dirty(direct_read, 9, flush_once).is_error()
            || Kernel::write_back_block_if_dirty(direct_read, 9, flush_once).is_error()
            || flush_count != 1 || direct_read.cache->entries[0].dirty)
            return 1;
    }
    return 0;
}
