/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/MemoryStream.h>
#include <LibCompress/Zlib.h>
#include <LibCore/MappedFile.h>
#include <LibGfx/Bitmap.h>
#include <LibGfx/ImageFormats/PNGLoader.h>
#include <LibGfx/ImageFormats/PNGWriter.h>
#include <LibTest/TestCase.h>
#include <string.h>

#ifdef AK_OS_SERENITY
#    define TEST_INPUT(x) ("/usr/Tests/LibGfx/test-inputs/" x)
#else
#    define TEST_INPUT(x) ("test-inputs/" x)
#endif

TEST_CASE(png_pixels_have_native_argb32_colors)
{
    struct PNGCase {
        StringView path;
        u32 expected_argb;
    };
    PNGCase const cases[] {
        { TEST_INPUT("png/endian/rgba8.png"sv), 0x78123456 },
        { TEST_INPUT("png/endian/gray16.png"sv), 0xff121212 },
        { TEST_INPUT("png/endian/grayalpha16.png"sv), 0xab121212 },
        { TEST_INPUT("png/endian/rgb16.png"sv), 0xff12569a },
        { TEST_INPUT("png/endian/rgba16.png"sv), 0xde12569a },
        { TEST_INPUT("png/endian/rgb8_trns.png"sv), 0x00123456 },
        { TEST_INPUT("png/endian/rgb16_trns.png"sv), 0x0012569a },
        { TEST_INPUT("png/endian/indexed8.png"sv), 0x78123456 },
    };

    for (auto const& test : cases) {
        auto file = TRY_OR_FAIL(Core::MappedFile::map(test.path));
        auto decoder = TRY_OR_FAIL(Gfx::PNGImageDecoderPlugin::create(file->bytes()));
        auto frame = TRY_OR_FAIL(decoder->frame(0));
        EXPECT(frame.image);
        if (frame.image)
            EXPECT_EQ(frame.image->get_pixel(0, 0).value(), test.expected_argb);
    }
}

TEST_CASE(png_writer_emits_rgba_bytes)
{
    auto bitmap = TRY_OR_FAIL(Gfx::Bitmap::create(Gfx::BitmapFormat::BGRA8888, { 1, 1 }));
    bitmap->set_pixel(0, 0, Gfx::Color::from_argb(0x78123456));
    auto encoded = TRY_OR_FAIL(Gfx::PNGWriter::encode(*bitmap));
    auto bytes = encoded.bytes();

    bool found_image_data = false;
    for (size_t offset = 8; offset + 12 <= bytes.size();) {
        size_t chunk_size = (static_cast<size_t>(bytes[offset]) << 24)
            | (static_cast<size_t>(bytes[offset + 1]) << 16)
            | (static_cast<size_t>(bytes[offset + 2]) << 8)
            | bytes[offset + 3];
        if (chunk_size > bytes.size() - offset - 12)
            break;

        if (memcmp(bytes.data() + offset + 4, "IDAT", 4) == 0) {
            auto stream = make<FixedMemoryStream>(bytes.slice(offset + 8, chunk_size));
            auto decompressor = TRY_OR_FAIL(Compress::ZlibDecompressor::create(move(stream)));
            auto scanline = TRY_OR_FAIL(decompressor->read_until_eof());
            u8 const rgba[] { 0x12, 0x34, 0x56, 0x78 };
            EXPECT_EQ(scanline.size(), sizeof(rgba) + 1);
            if (scanline.size() == sizeof(rgba) + 1)
                EXPECT_EQ(scanline.bytes().slice(1), (ReadonlyBytes { rgba, sizeof(rgba) }));
            found_image_data = true;
            break;
        }
        offset += chunk_size + 12;
    }
    EXPECT(found_image_data);
}
