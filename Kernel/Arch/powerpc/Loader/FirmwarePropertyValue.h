/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>

namespace Kernel::PowerPC::Loader {

template<typename Writer>
void write_firmware_property_value(Writer&& write, u8 const* value, u32 reported_length, u32 buffer_capacity, u32 full_length)
{
    // SmartFirmware can return the full property length after copying only buffer_capacity bytes.
    u32 length = reported_length < buffer_capacity ? reported_length : buffer_capacity;
    auto write_literal = [&](char const* text) {
        u32 size = 0;
        while (text[size] != 0)
            ++size;
        write(text, size);
    };
    auto write_word = [&](u32 word) {
        char output[] = "0x00000000";
        char const* digits = "0123456789abcdef";
        for (int index = 9; index >= 2; --index) {
            output[index] = digits[word & 15u];
            word >>= 4;
        }
        write(output, 10);
    };

    bool printable = length > 0 && value[length - 1] == 0;
    for (u32 i = 0; printable && i < length; ++i) {
        if (value[i] == 0 ? (i == 0 || value[i - 1] == 0) : (value[i] < 0x20 || value[i] > 0x7e))
            printable = false;
    }
    if (printable) {
        write_literal("\"");
        for (u32 start = 0; start < length;) {
            u32 end = start;
            while (end < length && value[end] != 0)
                ++end;
            if (start != 0)
                write_literal("\" \"");
            write(reinterpret_cast<char const*>(value + start), end - start);
            start = end + 1;
        }
        write_literal("\"");
    } else {
        write_literal("<");
        for (u32 offset = 0; offset + 4 <= length; offset += 4) {
            if (offset != 0)
                write_literal(" ");
            write_word((static_cast<u32>(value[offset]) << 24) | (static_cast<u32>(value[offset + 1]) << 16)
                | (static_cast<u32>(value[offset + 2]) << 8) | value[offset + 3]);
        }
        for (u32 offset = length & ~3u; offset < length; ++offset) {
            char byte[3] = { ' ', "0123456789abcdef"[value[offset] >> 4], "0123456789abcdef"[value[offset] & 15] };
            write(byte, sizeof(byte));
        }
        write_literal(">");
    }
    if (full_length > length) {
        write_literal(" (");
        write_word(full_length);
        write_literal(" bytes)");
    }
}

}
