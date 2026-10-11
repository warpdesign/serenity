/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/BitCast.h>
#include <AK/MemoryStream.h>
#include <LibGfx/ImageFormats/JPEGXL/Common.h>
#include <LibTest/TestCase.h>

static u32 decode_half_bits(u16 bits)
{
    u8 bytes[] { static_cast<u8>(bits), static_cast<u8>(bits >> 8) };
    FixedMemoryStream input { ReadonlyBytes { bytes, sizeof(bytes) } };
    LittleEndianInputBitStream stream { MaybeOwned<Stream>(input) };
    return bit_cast<u32>(MUST(Gfx::JPEGXL::F16(stream)));
}

TEST_CASE(jpeg_xl_half_float_values)
{
    EXPECT_EQ(decode_half_bits(0x0000), 0x00000000u);
    EXPECT_EQ(decode_half_bits(0x8000), 0x80000000u);
    EXPECT_EQ(decode_half_bits(0x3c00), 0x3f800000u); // 1
    EXPECT_EQ(decode_half_bits(0xbc00), 0xbf800000u); // -1
    EXPECT_EQ(decode_half_bits(0x3800), 0x3f000000u); // 0.5
    EXPECT_EQ(decode_half_bits(0x7bff), 0x477fe000u); // Largest finite half
    EXPECT_EQ(decode_half_bits(0x0400), 0x38800000u); // Smallest normal half
    EXPECT_EQ(decode_half_bits(0x03ff), 0x387fc000u); // Largest subnormal half
    EXPECT_EQ(decode_half_bits(0x0001), 0x33800000u); // Smallest subnormal half
    EXPECT_EQ(decode_half_bits(0x8001), 0xb3800000u); // Negative subnormal half
}
