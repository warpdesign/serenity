/*
 * Copyright (c) 2021-2026, Valtteri Koskivuori <vkoskiv@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/NumberFormat.h>
#include <AK/String.h>
#include <AK/Vector.h>
#include <LibArchive/Zip.h>
#include <LibAudio/Loader.h>
#include <LibCompress/Gzip.h>
#include <LibCore/ArgsParser.h>
#include <LibCore/File.h>
#include <LibCore/MappedFile.h>
#include <LibCore/MimeData.h>
#include <LibCore/System.h>
#include <LibELF/Image.h>
#include <LibGfx/ImageFormats/ImageDecoder.h>
#include <LibMain/Main.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

static ErrorOr<Optional<String>> description_only(StringView description, StringView)
{
    return String::from_utf8(description);
}

// FIXME: Ideally Gfx::ImageDecoder could tell us the image type directly.
static ErrorOr<Optional<String>> image_details(StringView description, StringView path)
{
    auto mapped_file = TRY(Core::MappedFile::map(path));
    auto mime_type = Core::guess_mime_type_based_on_filename(path);
    auto image_decoder = TRY(Gfx::ImageDecoder::try_create_for_raw_bytes(mapped_file->bytes(), mime_type));
    if (!image_decoder)
        return OptionalNone {};

    StringBuilder builder;
    builder.appendff("{}, {} x {}", description, image_decoder->width(), image_decoder->height());
    if (image_decoder->is_animated()) {
        builder.appendff(", animated with {} frames that loop", image_decoder->frame_count());
        int loop_count = image_decoder->loop_count();
        if (loop_count == 0)
            builder.appendff(" indefinitely");
        else
            builder.appendff(" {} {}", loop_count, loop_count == 1 ? "time" : "times");
    }

    return builder.to_string();
}

static ErrorOr<Optional<String>> audio_details(StringView description, StringView path)
{
    auto loader_or_error = Audio::Loader::create(path);
    if (loader_or_error.is_error())
        return OptionalNone {};

    auto loader = loader_or_error.release_value();
    StringBuilder builder;
    builder.appendff("{}, {} Hz, {}-bit {}, {} samples ({} s)",
        description,
        loader->sample_rate(),
        loader->bits_per_sample(),
        loader->num_channels() == 1 ? "Mono" : "Stereo",
        loader->total_samples(),
        loader->total_samples() / loader->sample_rate());
    auto metadata = loader->metadata();
    Vector<String> metadata_parts;
    if (metadata.title.has_value()) {
        // FIXME: Use “pretty quotation” once our terminal fonts support these characters.
        if (auto title_text = String::formatted("\"{}\"", metadata.title.value_or({})); !title_text.is_error()) {
            // We intentionally discard the error, because not printing part of the metadata due to OOM is not a problem.
            (void)metadata_parts.try_append(title_text.release_value());
        }
    }
    if (metadata.album.has_value()) {
        if (auto album_text = String::formatted("(Album: {})", metadata.album.value_or({})); !album_text.is_error())
            (void)metadata_parts.try_append(album_text.release_value());
    }
    if (auto all_artists = metadata.all_artists(); !all_artists.is_error() && all_artists.value().has_value()) {
        if (auto artist_text = String::formatted("by {}", all_artists.release_value().release_value()); !artist_text.is_error())
            (void)metadata_parts.try_append(artist_text.release_value());
    }

    if (!metadata_parts.is_empty()) {
        // New line for the metadata.
        builder.append_code_point('\n');
        builder.join(" "sv, metadata_parts);
    }

    return builder.to_string();
}

static ErrorOr<Optional<String>> gzip_details(StringView description, StringView path)
{
    auto mapped_file = TRY(Core::MappedFile::map(path));
    if (!Compress::GzipDecompressor::is_likely_compressed(mapped_file->bytes()))
        return OptionalNone {};

    auto gzip_details = TRY(Compress::GzipDecompressor::describe_header(mapped_file->bytes()));
    if (!gzip_details.has_value())
        return OptionalNone {};

    return TRY(String::formatted("{}, {}", description, gzip_details.value()));
}

static ErrorOr<Optional<String>> zip_details(StringView description, StringView path)
{
    auto mapped_file = TRY(Core::MappedFile::map(path));
    auto zip_file = Archive::Zip::try_create(mapped_file->bytes());
    auto statistics = TRY(zip_file->calculate_statistics());
    return TRY(String::formatted("{}, {} {}, {} {} totaling {} uncompressed",
        description,
        statistics.directory_count(),
        statistics.directory_count() == 1 ? "directory" : "directories",
        statistics.file_count(),
        statistics.file_count() == 1 ? "file" : "files",
        AK::human_readable_size(statistics.total_uncompressed_bytes())));
}

static Optional<u64> read_elf_integer(ReadonlyBytes bytes, size_t offset, size_t size, bool little_endian)
{
    if (offset > bytes.size() || size > bytes.size() - offset)
        return {};
    u64 value = 0;
    for (size_t index = 0; index < size; ++index) {
        auto byte = bytes[offset + (little_endian ? size - index - 1 : index)];
        value = (value << 8) | byte;
    }
    return value;
}

static ErrorOr<Optional<String>> elf_details(StringView description, StringView path)
{
    auto mapped_file = TRY(Core::MappedFile::map(path));
    auto elf_data = mapped_file->bytes();
    if (elf_data.size() < EI_NIDENT || elf_data[EI_MAG0] != ELFMAG0 || elf_data[EI_MAG1] != ELFMAG1
        || elf_data[EI_MAG2] != ELFMAG2 || elf_data[EI_MAG3] != ELFMAG3)
        return OptionalNone {};

    bool is_64_bit = elf_data[EI_CLASS] == ELFCLASS64;
    if (!is_64_bit && elf_data[EI_CLASS] != ELFCLASS32)
        return OptionalNone {};
    bool little_endian = elf_data[EI_DATA] == ELFDATA2LSB;
    if (!little_endian && elf_data[EI_DATA] != ELFDATA2MSB)
        return OptionalNone {};

    size_t header_size = is_64_bit ? 64 : 52;
    size_t program_header_size = is_64_bit ? 56 : 32;
    if (elf_data.size() < header_size || elf_data[EI_VERSION] != EV_CURRENT)
        return OptionalNone {};

    auto read = [&](size_t offset, size_t size) { return read_elf_integer(elf_data, offset, size, little_endian); };
    auto type = read(16, 2);
    auto machine = read(18, 2);
    auto version = read(20, 4);
    auto program_header_offset = read(is_64_bit ? 32 : 28, is_64_bit ? 8 : 4);
    auto declared_header_size = read(is_64_bit ? 52 : 40, 2);
    auto declared_program_header_size = read(is_64_bit ? 54 : 42, 2);
    auto program_header_count = read(is_64_bit ? 56 : 44, 2);
    if (!type.has_value() || !machine.has_value() || version != static_cast<u64>(EV_CURRENT)
        || !program_header_offset.has_value() || !declared_header_size.has_value()
        || !declared_program_header_size.has_value() || !program_header_count.has_value()
        || *declared_header_size < header_size)
        return OptionalNone {};

    StringView interpreter_path;
    if (*program_header_count != 0) {
        if (*declared_program_header_size < program_header_size
            || *program_header_offset > elf_data.size()
            || *program_header_count > (elf_data.size() - *program_header_offset) / *declared_program_header_size)
            return OptionalNone {};
        for (size_t index = 0; index < *program_header_count; ++index) {
            size_t offset = *program_header_offset + index * *declared_program_header_size;
            if (read(offset, 4) != static_cast<u64>(PT_INTERP))
                continue;
            auto string_offset = read(offset + (is_64_bit ? 8 : 4), is_64_bit ? 8 : 4);
            auto string_size = read(offset + (is_64_bit ? 32 : 16), is_64_bit ? 8 : 4);
            if (!string_offset.has_value() || !string_size.has_value() || *string_size == 0
                || *string_offset > elf_data.size() || *string_size > elf_data.size() - *string_offset
                || elf_data[*string_offset + *string_size - 1] != '\0')
                return OptionalNone {};
            interpreter_path = { reinterpret_cast<char const*>(elf_data.data() + *string_offset), static_cast<size_t>(*string_size - 1) };
            break;
        }
    }

    auto bitness = is_64_bit ? "64" : "32";
    auto byteorder = little_endian ? "LSB" : "MSB";

    bool is_dynamically_linked = !interpreter_path.is_empty();
    auto dynamic_section = TRY(String::formatted(", dynamically linked, interpreter {}", interpreter_path));

    return TRY(String::formatted("{} {}-bit {} {}, {}, version {} ({}){}",
        description,
        bitness,
        byteorder,
        ELF::Image::object_file_type_to_string(*type).value_or("(?)"sv),
        ELF::Image::object_machine_type_to_string(*machine).value_or("(?)"sv),
        elf_data[EI_ABIVERSION],
        ELF::Image::object_abi_type_to_string(elf_data[EI_OSABI]).value_or("(?)"sv),
        is_dynamically_linked ? dynamic_section : String {}));
}

struct PatternAndFunction {
    StringView matching_pattern;
    ErrorOr<Optional<String>> (*details)(StringView, StringView);
};

static constexpr Array s_pattern_with_specialized_functions {
    PatternAndFunction { "application/gzip"sv, gzip_details },
    PatternAndFunction { "application/zip"sv, zip_details },
    PatternAndFunction { "extra/elf"sv, elf_details },
    PatternAndFunction { "audio/*"sv, audio_details },
    PatternAndFunction { "image/*"sv, image_details },
    PatternAndFunction { "application/dicom"sv, image_details },
};

static ErrorOr<Optional<String>> get_description_from_mime_type(StringView mime, StringView path)
{
    auto const mime_type = Core::get_mime_type_data(mime);

    if (!mime_type.has_value())
        return OptionalNone {};

    for (auto const& pattern : s_pattern_with_specialized_functions) {
        if (mime.matches(pattern.matching_pattern))
            return pattern.details(mime_type->description, path);
    }

    return description_only(mime_type->description, path);
}

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    TRY(Core::System::pledge("stdio rpath"));

    Vector<StringView> paths;
    bool flag_mime_only = false;
    bool flag_brief_mode = false;

    Core::ArgsParser args_parser;
    args_parser.set_general_help("Determine type of files");
    args_parser.add_option(flag_mime_only, "Only print mime type", "mime-type", 'I');
    args_parser.add_option(flag_brief_mode, "Do not prepend file names to output lines", "brief", 'b');
    args_parser.add_positional_argument(paths, "Files to identify", "files", Core::ArgsParser::Required::Yes);
    args_parser.parse(arguments);

    bool all_ok = true;

    for (auto const& path : paths) {
        auto file_or_error = Core::File::open(path, Core::File::OpenMode::Read);
        if (file_or_error.is_error()) {
            warnln("{}: {}", path, file_or_error.release_error());
            all_ok = false;
            continue;
        }
        auto file = file_or_error.release_value();

        struct stat file_stat = TRY(Core::System::lstat(path));

        if (!flag_brief_mode)
            out("{}: ", path);

        auto file_size_in_bytes = file_stat.st_size;
        if (S_ISDIR(file_stat.st_mode)) {
            outln("directory");
        } else if (!file_size_in_bytes) {
            outln("empty");
        } else {
            auto file_name_guess = Core::guess_mime_type_based_on_filename(path);
            auto mime_type = Core::guess_mime_type_based_on_sniffed_bytes(*file).value_or(file_name_guess);
            auto human_readable_description = TRY(get_description_from_mime_type(mime_type, path));
            if (!human_readable_description.has_value())
                human_readable_description = TRY(String::from_utf8(mime_type));
            outln("{}", flag_mime_only ? mime_type : *human_readable_description);
        }
    }

    return all_ok ? 0 : 1;
}
