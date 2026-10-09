/*
 * Copyright (c) 2023-2026, Dan Klishch <danilklishch@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Badge.h>
#include <AK/Endian.h>
#include <AK/Forward.h>

namespace AK::Detail {

class StringData;

static constexpr size_t MAX_SHORT_STRING_BYTE_COUNT = sizeof(StringData*) - 1;

struct ShortString {
    // The flag overlays the least significant pointer byte in the union.
    static constexpr size_t flag_byte_offset = HostIsLittleEndian ? 0 : MAX_SHORT_STRING_BYTE_COUNT;
    static constexpr size_t storage_byte_offset = HostIsLittleEndian ? 1 : 0;

    constexpr u8& flag() { return raw[flag_byte_offset]; }
    constexpr u8 const& flag() const { return raw[flag_byte_offset]; }
    constexpr u8* storage() { return raw + storage_byte_offset; }
    constexpr u8 const* storage() const { return raw + storage_byte_offset; }

    ReadonlyBytes bytes() const;
    size_t byte_count() const;

    // The flag byte stores the byte count shifted left once, with bit zero set.
    u8 raw[sizeof(StringData*)] = { 0 };
};

static_assert(sizeof(ShortString) == sizeof(StringData*));
static_assert(__builtin_offsetof(ShortString, raw) == 0);

class StringBase {
public:
    // Creates an empty (zero-length) String.
    constexpr StringBase()
        : StringBase(empty_short_string())
    {
    }

    StringBase(StringBase const&);

    constexpr StringBase(StringBase&& other)
        : m_short_string(other.m_short_string)
    {
        other.m_short_string = ShortString {};
        other.m_short_string.flag() = SHORT_STRING_FLAG;
    }

    StringBase& operator=(StringBase&&);
    StringBase& operator=(StringBase const&);

    constexpr ~StringBase()
    {
        if (!is_constant_evaluated())
            destroy_string();
    }

    // NOTE: This is primarily interesting to unit tests.
    [[nodiscard]] constexpr bool is_short_string() const
    {
        return (m_short_string.flag() & SHORT_STRING_FLAG) != 0;
    }

    // Returns the underlying UTF-8 encoded bytes.
    // NOTE: There is no guarantee about null-termination.
    [[nodiscard]] ReadonlyBytes bytes() const;
    [[nodiscard]] u32 hash() const;
    [[nodiscard]] size_t byte_count() const;

    [[nodiscard]] bool operator==(StringBase const&) const;

    [[nodiscard]] ALWAYS_INLINE FlatPtr raw(Badge<FlyString>) const { return bit_cast<FlatPtr>(m_data); }

protected:
    template<typename Func>
    ErrorOr<void> replace_with_new_string(size_t byte_count, Func&& callback)
    {
        Bytes buffer = TRY(replace_with_uninitialized_buffer(byte_count));
        if (byte_count != 0)
            TRY(callback(buffer));
        return {};
    }

    template<typename Func>
    constexpr void replace_with_new_short_string(size_t byte_count, Func&& callback)
    {
        Bytes buffer = replace_with_uninitialized_short_string(byte_count);
        if (byte_count != 0)
            callback(buffer);
    }

    // This is not a trivial operation with storage, so it does not belong here. Unfortunately, it
    // is impossible to implement it without access to StringData.
    ErrorOr<StringBase> substring_from_byte_offset_with_shared_superstring(size_t start, size_t byte_count) const;

private:
    friend class ::AK::String;
    friend class ::AK::FlyString;

    // NOTE: If the least significant bit of the pointer is set, this is a short string.
    static constexpr uintptr_t SHORT_STRING_FLAG = 1;

    static constexpr ShortString empty_short_string()
    {
        ShortString string {};
        string.flag() = SHORT_STRING_FLAG;
        return string;
    }

    explicit StringBase(NonnullRefPtr<Detail::StringData const>);

    explicit constexpr StringBase(ShortString short_string)
        : m_short_string(short_string)
    {
    }

    ErrorOr<Bytes> replace_with_uninitialized_buffer(size_t byte_count);

    constexpr Bytes replace_with_uninitialized_short_string(size_t byte_count)
    {
        VERIFY(is_short_string());
        VERIFY(byte_count <= MAX_SHORT_STRING_BYTE_COUNT);

        m_short_string = ShortString {};
        m_short_string.flag() = (byte_count << 1) | SHORT_STRING_FLAG;
        return { m_short_string.storage(), byte_count };
    }

    void destroy_string();

    union {
        ShortString m_short_string;
        Detail::StringData const* m_data { nullptr };
    };
};

}
