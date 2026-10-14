/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/Loader/Inflate.h>

namespace Kernel::PowerPC::Loader {

namespace {

constexpr u32 max_code_length = 15;
constexpr u32 max_literal_length_codes = 286;
constexpr u32 max_distance_codes = 30;
constexpr u32 max_codes = max_literal_length_codes + max_distance_codes;

class BitReader {
public:
    BitReader(u8 const* data, u32 size)
        : m_data(data)
        , m_size(size)
    {
    }

    // Deflate packs values starting at the least significant bit of each byte.
    u32 bits(u32 count)
    {
        while (m_bit_count < count) {
            if (m_offset >= m_size) {
                m_failed = true;
                return 0;
            }
            m_bit_buffer |= static_cast<u32>(m_data[m_offset++]) << m_bit_count;
            m_bit_count += 8;
        }
        u32 value = m_bit_buffer & ((1u << count) - 1);
        m_bit_buffer = count == 32 ? 0 : m_bit_buffer >> count;
        m_bit_count -= count;
        return value;
    }

    void align_to_byte()
    {
        m_bit_buffer = 0;
        m_bit_count = 0;
    }

    bool read_bytes(u8* output, u32 count)
    {
        if (count > m_size - m_offset)
            return false;
        for (u32 i = 0; i < count; ++i)
            output[i] = m_data[m_offset + i];
        m_offset += count;
        return true;
    }

    bool failed() const { return m_failed; }

private:
    u8 const* m_data;
    u32 m_size;
    u32 m_offset { 0 };
    u32 m_bit_buffer { 0 };
    u32 m_bit_count { 0 };
    bool m_failed { false };
};

// Canonical Huffman code: the number of codes of each length, and the symbols in code order.
struct HuffmanCode {
    u16 count[max_code_length + 1];
    u16 symbol[max_codes];
};

bool build_code(HuffmanCode& code, u8 const* lengths, u32 symbol_count)
{
    for (auto& count : code.count)
        count = 0;
    for (u32 i = 0; i < symbol_count; ++i)
        ++code.count[lengths[i]];
    if (code.count[0] == symbol_count)
        return true;

    // Reject over-subscribed codes; incomplete ones are allowed, as zlib does for single-code trees.
    int left = 1;
    for (u32 length = 1; length <= max_code_length; ++length) {
        left <<= 1;
        left -= code.count[length];
        if (left < 0)
            return false;
    }

    u16 offsets[max_code_length + 1];
    offsets[1] = 0;
    for (u32 length = 1; length < max_code_length; ++length)
        offsets[length + 1] = offsets[length] + code.count[length];
    for (u32 i = 0; i < symbol_count; ++i) {
        if (lengths[i] != 0)
            code.symbol[offsets[lengths[i]]++] = static_cast<u16>(i);
    }
    return true;
}

int decode_symbol(BitReader& reader, HuffmanCode const& code)
{
    int value = 0;
    int first = 0;
    int index = 0;
    for (u32 length = 1; length <= max_code_length; ++length) {
        value |= static_cast<int>(reader.bits(1));
        int count = code.count[length];
        if (value - count < first)
            return code.symbol[index + (value - first)];
        index += count;
        first += count;
        first <<= 1;
        value <<= 1;
    }
    return -1;
}

class Output {
public:
    Output(u8* data, u32 size)
        : m_data(data)
        , m_size(size)
    {
    }

    bool put(u8 byte)
    {
        if (m_offset >= m_size)
            return false;
        m_data[m_offset++] = byte;
        return true;
    }

    bool copy_back(u32 distance, u32 length)
    {
        if (distance == 0 || distance > m_offset || length > m_size - m_offset)
            return false;
        // Byte by byte, since the source may overlap the bytes being written.
        for (u32 i = 0; i < length; ++i, ++m_offset)
            m_data[m_offset] = m_data[m_offset - distance];
        return true;
    }

    u8* cursor() { return m_data + m_offset; }
    bool advance(u32 count)
    {
        if (count > m_size - m_offset)
            return false;
        m_offset += count;
        return true;
    }

    u32 size() const { return m_offset; }
    u8 const* data() const { return m_data; }

private:
    u8* m_data;
    u32 m_size;
    u32 m_offset { 0 };
};

constexpr u16 length_base[] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
constexpr u8 length_extra_bits[] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
constexpr u16 distance_base[] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
constexpr u8 distance_extra_bits[] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

bool inflate_codes(BitReader& reader, Output& output, HuffmanCode const& literal_length_code, HuffmanCode const& distance_code)
{
    while (true) {
        int symbol = decode_symbol(reader, literal_length_code);
        if (symbol < 0 || reader.failed())
            return false;
        if (symbol < 256) {
            if (!output.put(static_cast<u8>(symbol)))
                return false;
            continue;
        }
        if (symbol == 256)
            return true;
        symbol -= 257;
        if (symbol >= 29)
            return false;
        u32 length = length_base[symbol] + reader.bits(length_extra_bits[symbol]);
        int distance_symbol = decode_symbol(reader, distance_code);
        if (distance_symbol < 0 || distance_symbol >= 30)
            return false;
        u32 distance = distance_base[distance_symbol] + reader.bits(distance_extra_bits[distance_symbol]);
        if (reader.failed() || !output.copy_back(distance, length))
            return false;
    }
}

HuffmanCode s_literal_length_code;
HuffmanCode s_distance_code;

bool inflate_fixed(BitReader& reader, Output& output)
{
    u8 lengths[288];
    for (u32 i = 0; i < 144; ++i)
        lengths[i] = 8;
    for (u32 i = 144; i < 256; ++i)
        lengths[i] = 9;
    for (u32 i = 256; i < 280; ++i)
        lengths[i] = 7;
    for (u32 i = 280; i < 288; ++i)
        lengths[i] = 8;
    if (!build_code(s_literal_length_code, lengths, 288))
        return false;
    for (u32 i = 0; i < max_distance_codes; ++i)
        lengths[i] = 5;
    if (!build_code(s_distance_code, lengths, max_distance_codes))
        return false;
    return inflate_codes(reader, output, s_literal_length_code, s_distance_code);
}

bool inflate_dynamic(BitReader& reader, Output& output)
{
    static constexpr u8 code_length_order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
    u32 literal_length_count = reader.bits(5) + 257;
    u32 distance_count = reader.bits(5) + 1;
    u32 code_length_count = reader.bits(4) + 4;
    if (reader.failed() || literal_length_count > max_literal_length_codes || distance_count > max_distance_codes)
        return false;

    u8 lengths[max_codes];
    for (u32 i = 0; i < 19; ++i)
        lengths[code_length_order[i]] = i < code_length_count ? static_cast<u8>(reader.bits(3)) : 0;
    if (reader.failed() || !build_code(s_literal_length_code, lengths, 19))
        return false;

    u32 index = 0;
    while (index < literal_length_count + distance_count) {
        int symbol = decode_symbol(reader, s_literal_length_code);
        if (symbol < 0 || reader.failed())
            return false;
        if (symbol < 16) {
            lengths[index++] = static_cast<u8>(symbol);
            continue;
        }
        u8 repeated = 0;
        u32 repeat;
        if (symbol == 16) {
            if (index == 0)
                return false;
            repeated = lengths[index - 1];
            repeat = 3 + reader.bits(2);
        } else if (symbol == 17) {
            repeat = 3 + reader.bits(3);
        } else {
            repeat = 11 + reader.bits(7);
        }
        if (reader.failed() || index + repeat > literal_length_count + distance_count)
            return false;
        while (repeat--)
            lengths[index++] = repeated;
    }
    // The end-of-block code must be present.
    if (lengths[256] == 0)
        return false;
    if (!build_code(s_literal_length_code, lengths, literal_length_count)
        || !build_code(s_distance_code, lengths + literal_length_count, distance_count))
        return false;
    return inflate_codes(reader, output, s_literal_length_code, s_distance_code);
}

bool inflate(BitReader& reader, Output& output)
{
    bool last_block;
    do {
        last_block = reader.bits(1);
        u32 type = reader.bits(2);
        if (reader.failed())
            return false;
        bool ok;
        if (type == 0) {
            reader.align_to_byte();
            u8 header[4];
            if (!reader.read_bytes(header, 4))
                return false;
            u32 length = header[0] | (header[1] << 8);
            u32 inverted = header[2] | (header[3] << 8);
            if (length != (~inverted & 0xffffu))
                return false;
            u8* destination = output.cursor();
            ok = output.advance(length) && reader.read_bytes(destination, length);
        } else if (type == 1) {
            ok = inflate_fixed(reader, output);
        } else if (type == 2) {
            ok = inflate_dynamic(reader, output);
        } else {
            return false;
        }
        if (!ok)
            return false;
    } while (!last_block);
    return true;
}

u32 s_crc_table[256];

u32 crc32(u8 const* data, u32 size)
{
    if (s_crc_table[1] == 0) {
        for (u32 i = 0; i < 256; ++i) {
            u32 value = i;
            for (u32 bit = 0; bit < 8; ++bit)
                value = (value & 1) ? (value >> 1) ^ 0xedb88320u : value >> 1;
            s_crc_table[i] = value;
        }
    }
    u32 crc = 0xffffffffu;
    for (u32 i = 0; i < size; ++i)
        crc = s_crc_table[(crc ^ data[i]) & 0xff] ^ (crc >> 8);
    return crc ^ 0xffffffffu;
}

u32 read_le32(u8 const* data)
{
    return data[0] | (data[1] << 8) | (data[2] << 16) | (static_cast<u32>(data[3]) << 24);
}

constexpr u32 gzip_header_size = 10;
constexpr u32 gzip_trailer_size = 8;

// Returns the offset of the deflate stream, or 0 if the header is invalid.
u32 gzip_payload_offset(u8 const* data, u32 size)
{
    constexpr u8 flag_header_crc = 1 << 1;
    constexpr u8 flag_extra = 1 << 2;
    constexpr u8 flag_name = 1 << 3;
    constexpr u8 flag_comment = 1 << 4;
    if (size < gzip_header_size + gzip_trailer_size || data[0] != 0x1f || data[1] != 0x8b || data[2] != 8)
        return 0;
    u8 flags = data[3];
    u32 offset = gzip_header_size;
    if (flags & flag_extra) {
        if (offset + 2 > size)
            return 0;
        offset += 2 + (data[offset] | (data[offset + 1] << 8));
    }
    auto skip_zero_terminated = [&] {
        while (offset < size && data[offset] != 0)
            ++offset;
        ++offset;
    };
    if (flags & flag_name)
        skip_zero_terminated();
    if (flags & flag_comment)
        skip_zero_terminated();
    if (flags & flag_header_crc)
        offset += 2;
    if (offset + gzip_trailer_size > size)
        return 0;
    return offset;
}

}

u32 gzip_uncompressed_size(u8 const* data, u32 size)
{
    if (gzip_payload_offset(data, size) == 0)
        return 0;
    return read_le32(data + size - 4);
}

bool gzip_inflate(u8 const* data, u32 size, u8* output, u32 output_size)
{
    u32 payload = gzip_payload_offset(data, size);
    if (payload == 0)
        return false;
    BitReader reader { data + payload, size - payload - gzip_trailer_size };
    Output out { output, output_size };
    if (!inflate(reader, out))
        return false;
    u32 expected_crc = read_le32(data + size - 8);
    u32 expected_size = read_le32(data + size - 4);
    return out.size() == expected_size && out.size() == output_size && crc32(out.data(), out.size()) == expected_crc;
}

}
