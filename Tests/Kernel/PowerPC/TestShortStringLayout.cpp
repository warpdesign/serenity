/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/StringBase.h>

static_assert(!AK::HostIsLittleEndian);
static_assert(sizeof(AK::Detail::ShortString) == sizeof(void*));

static_assert([] {
    AK::Detail::StringBase string;
    return string.is_short_string();
}());

extern "C" int powerpc_short_string_layout_test()
{
    AK::Detail::ShortString string {};
    string.flag() = (3 << 1) | 1;
    auto* storage = string.storage();
    storage[0] = 'a';
    storage[1] = 'b';
    storage[2] = 'c';

    unsigned int tagged_word;
    __builtin_memcpy(&tagged_word, &string, sizeof(tagged_word));
    return tagged_word == 0x61626307u && (string.flag() >> 1) == 3
        && storage[0] == 'a' && storage[1] == 'b' && storage[2] == 'c';
}
