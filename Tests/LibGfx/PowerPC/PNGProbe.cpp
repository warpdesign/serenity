/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/MemoryStream.h>
#include <LibCompress/Zlib.h>
#include <LibGfx/Bitmap.h>
#include <LibGfx/ImageFormats/PNGLoader.h>
#include <LibGfx/ImageFormats/PNGWriter.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

[[noreturn]] static void report_and_wait(char const* message)
{
    dbgputstr(message, strlen(message));
    for (;;)
        asm volatile("" ::: "memory");
}

static bool writer_emits_rgba_bytes()
{
    auto bitmap = Gfx::Bitmap::create(Gfx::BitmapFormat::BGRA8888, { 1, 1 });
    if (bitmap.is_error())
        return false;
    bitmap.value()->set_pixel(0, 0, Gfx::Color::from_argb(0x78123456));
    auto encoded = Gfx::PNGWriter::encode(*bitmap.value());
    if (encoded.is_error())
        return false;
    auto bytes = encoded.value().bytes();

    for (size_t offset = 8; offset + 12 <= bytes.size();) {
        size_t chunk_size = (static_cast<size_t>(bytes[offset]) << 24)
            | (static_cast<size_t>(bytes[offset + 1]) << 16)
            | (static_cast<size_t>(bytes[offset + 2]) << 8)
            | bytes[offset + 3];
        if (chunk_size > bytes.size() - offset - 12)
            return false;
        if (memcmp(bytes.data() + offset + 4, "IDAT", 4) == 0) {
            auto stream = make<FixedMemoryStream>(bytes.slice(offset + 8, chunk_size));
            auto decompressor = Compress::ZlibDecompressor::create(move(stream));
            if (decompressor.is_error())
                return false;
            auto scanline = decompressor.value()->read_until_eof();
            if (scanline.is_error() || scanline.value().size() != 5)
                return false;
            u8 const expected[] { 0x12, 0x34, 0x56, 0x78 };
            return memcmp(scanline.value().data() + 1, expected, sizeof(expected)) == 0;
        }
        offset += chunk_size + 12;
    }
    return false;
}

extern "C" [[noreturn]] void _start()
{
    dbgputstr("PPC PNG entry\n", sizeof("PPC PNG entry\n") - 1);
    int fd = open("/cursor.png", O_RDONLY);
    if (fd < 0)
        report_and_wait("PowerPC PNG cursor open failed\n");

    u8 data[4096];
    ssize_t size = read(fd, data, sizeof(data));
    close(fd);
    if (size <= 0)
        report_and_wait("PowerPC PNG cursor read failed\n");
    dbgputstr("PPC PNG read\n", sizeof("PPC PNG read\n") - 1);

    auto decoder = Gfx::PNGImageDecoderPlugin::create({ data, static_cast<size_t>(size) });
    if (decoder.is_error())
        report_and_wait("PowerPC PNG cursor header failed\n");
    dbgputstr("PPC PNG header\n", sizeof("PPC PNG header\n") - 1);
    auto frame = decoder.value()->frame(0);
    if (frame.is_error()) {
        auto reason = frame.error().string_literal();
        dbgputstr(reason.characters_without_null_termination(), reason.length());
        dbgputstr("\n", 1);
        report_and_wait("PowerPC PNG cursor frame failed\n");
    }
    if (!frame.value().image)
        report_and_wait("PowerPC PNG cursor frame missing bitmap\n");

    auto& bitmap = *frame.value().image;
    if (bitmap.width() != 15 || bitmap.height() != 21 || bitmap.get_pixel(5, 3).value() != 0x1e000000)
        report_and_wait("PowerPC PNG cursor pixel failed\n");
    if (!writer_emits_rgba_bytes())
        report_and_wait("PowerPC PNG writer RGBA bytes failed\n");

    report_and_wait("PowerPC PNG cursor decode and writer passed\n");
}
