/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/ByteBuffer.h>
#include <AK/ByteString.h>
#include <AK/Function.h>
#include <AK/Memory.h>
#include <AK/Platform.h>
#include <LibCore/ArgsParser.h>
#include <LibCore/System.h>
#include <LibMain/Main.h>
#include <string.h>
#include <sys/devices/gpu.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

// Called through volatile pointers so the compiler cannot replace them with inline expansions.
static void* (*volatile s_memcpy)(void*, void const*, size_t) = memcpy;
static void* (*volatile s_memmove)(void*, void const*, size_t) = memmove;
static void* (*volatile s_memset)(void*, int, size_t) = memset;

// Results also go to the kernel debug log, which reaches the serial console on machines without a usable screen.
template<typename... Parameters>
static void say(CheckedFormatString<Parameters...>&& format, Parameters const&... parameters)
{
    auto line = ByteString::formatted(move(format), parameters...);
    outln("{}", line);
    dbgln("membench: {}", line);
}

static double now_seconds()
{
    timespec time {};
    clock_gettime(CLOCK_MONOTONIC, &time);
    return static_cast<double>(time.tv_sec) + static_cast<double>(time.tv_nsec) / 1e9;
}

// Repeats the operation for at least min_seconds and reports the best single-pass rate.
static void report(StringView name, size_t bytes_per_pass, Function<void()> operation, double min_seconds = 0.5)
{
    double best = 0;
    double start = now_seconds();
    size_t passes = 0;
    do {
        double pass_start = now_seconds();
        operation();
        double elapsed = now_seconds() - pass_start;
        if (elapsed > 0) {
            double rate = static_cast<double>(bytes_per_pass) / elapsed;
            if (rate > best)
                best = rate;
        }
        ++passes;
    } while (now_seconds() - start < min_seconds || passes < 2);
    say("{:<44} {:>9.1} MB/s  ({} passes of {} KiB)", name, best / 1e6, passes, bytes_per_pass / 1024);
}

static void fill_bytes_volatile(u8* destination, size_t size, u8 value)
{
    auto* pointer = reinterpret_cast<u8 volatile*>(destination);
    for (size_t i = 0; i < size; ++i)
        pointer[i] = value;
}

static void fill_words_volatile(u8* destination, size_t size, u32 value)
{
    auto* pointer = reinterpret_cast<u32 volatile*>(destination);
    for (size_t i = 0; i < size / 4; ++i)
        pointer[i] = value;
}

static u32 sum_words_volatile(u8 const* source, size_t size)
{
    auto const* pointer = reinterpret_cast<u32 const volatile*>(source);
    u32 sum = 0;
    for (size_t i = 0; i < size / 4; ++i)
        sum += pointer[i];
    return sum;
}

#if ARCH(POWERPC32)
// 8-byte floating-point stores are the widest single bus writes a G3 can issue.
static void copy_doubles(u8* destination, u8 const* source, size_t size)
{
    size_t blocks = size / 32;
    if (blocks == 0)
        return;
    asm volatile(
        "mtctr %2\n"
        "1:\n"
        "lfd 0, 0(%1)\n"
        "lfd 1, 8(%1)\n"
        "lfd 2, 16(%1)\n"
        "lfd 3, 24(%1)\n"
        "stfd 0, 0(%0)\n"
        "stfd 1, 8(%0)\n"
        "stfd 2, 16(%0)\n"
        "stfd 3, 24(%0)\n"
        "addi %1, %1, 32\n"
        "addi %0, %0, 32\n"
        "bdnz 1b\n"
        : "+b"(destination), "+b"(source)
        : "r"(blocks)
        : "ctr", "fr0", "fr1", "fr2", "fr3", "memory");
}
#endif

static int verify()
{
    static constexpr size_t max_length = 300;
    static constexpr size_t slack = 16;
    u8 source[max_length + 2 * slack];
    u8 destination[max_length + 2 * slack];
    u8 expected[max_length + 2 * slack];
    size_t failures = 0;
    auto fail = [&](StringView what, size_t a, size_t b, size_t length) {
        if (failures++ < 10)
            warnln("membench: {} failed (offsets {} {}, length {})", what, a, b, length);
    };

    for (size_t i = 0; i < sizeof(source); ++i)
        source[i] = static_cast<u8>(i * 7 + 3);

    for (size_t source_offset = 0; source_offset < 8; ++source_offset) {
        for (size_t destination_offset = 0; destination_offset < 8; ++destination_offset) {
            for (size_t length = 0; length <= max_length; ++length) {
                __builtin_memset(destination, 0xee, sizeof(destination));
                for (size_t i = 0; i < sizeof(expected); ++i)
                    expected[i] = 0xee;
                for (size_t i = 0; i < length; ++i)
                    expected[slack + destination_offset + i] = source[slack + source_offset + i];
                void* result = s_memcpy(destination + slack + destination_offset, source + slack + source_offset, length);
                if (result != destination + slack + destination_offset || __builtin_memcmp(destination, expected, sizeof(expected)) != 0)
                    fail("memcpy"sv, source_offset, destination_offset, length);
            }
        }
    }

    for (size_t offset = 0; offset < 8; ++offset) {
        for (size_t length = 0; length <= max_length; ++length) {
            for (int value : { 0, 0xa5, 0x1ff, -1 }) {
                __builtin_memset(destination, 0xee, sizeof(destination));
                for (size_t i = 0; i < sizeof(expected); ++i)
                    expected[i] = 0xee;
                for (size_t i = 0; i < length; ++i)
                    expected[slack + offset + i] = static_cast<u8>(value);
                void* result = s_memset(destination + slack + offset, value, length);
                if (result != destination + slack + offset || __builtin_memcmp(destination, expected, sizeof(expected)) != 0)
                    fail("memset"sv, offset, static_cast<size_t>(value), length);
            }
        }
    }

    // Overlapping moves in both directions, compared against a bytewise reference.
    for (size_t from = 0; from < 2 * slack; ++from) {
        for (size_t to = 0; to < 2 * slack; ++to) {
            for (size_t length = 0; length <= max_length; length += (length < 40 ? 1 : 13)) {
                for (size_t i = 0; i < sizeof(destination); ++i)
                    destination[i] = expected[i] = static_cast<u8>(i * 13 + 1);
                u8 temporary[max_length];
                for (size_t i = 0; i < length; ++i)
                    temporary[i] = expected[from + i];
                for (size_t i = 0; i < length; ++i)
                    expected[to + i] = temporary[i];
                void* result = s_memmove(destination + to, destination + from, length);
                if (result != destination + to || __builtin_memcmp(destination, expected, sizeof(expected)) != 0)
                    fail("memmove"sv, from, to, length);
            }
        }
    }

    if (failures != 0) {
        warnln("membench: {} verification failures", failures);
        return 1;
    }
    say("memcpy, memset and memmove verified");
    return 0;
}

static ErrorOr<void> benchmark_ram(size_t size)
{
    auto source = TRY(ByteBuffer::create_uninitialized(size));
    auto destination = TRY(ByteBuffer::create_uninitialized(size));
    s_memset(source.data(), 0x5a, size);
    s_memset(destination.data(), 0, size);

    say("RAM, {} KiB buffers:", size / 1024);
    report("  memcpy"sv, size, [&] { s_memcpy(destination.data(), source.data(), size); });
    report("  memcpy, source misaligned by 1"sv, size - 8, [&] { s_memcpy(destination.data(), source.data() + 1, size - 8); });
    report("  memset"sv, size, [&] { s_memset(destination.data(), 0, size); });
    report("  32-bit loads"sv, size, [&] { (void)sum_words_volatile(source.data(), size); });
    report("  32-bit stores"sv, size, [&] { fill_words_volatile(destination.data(), size, 0x12345678); });
    report("  8-bit stores"sv, size, [&] { fill_bytes_volatile(destination.data(), size, 0x12); });
#if ARCH(POWERPC32)
    report("  64-bit FP copy"sv, size, [&] { copy_doubles(destination.data(), source.data(), size); });
#endif
    return {};
}

static ErrorOr<void> benchmark_framebuffer(StringView path)
{
    int fd = TRY(Core::System::open(path, O_RDWR | O_CLOEXEC));
    GraphicsHeadModeSetting mode {};
    if (int rc = graphics_connector_get_head_mode_setting(fd, &mode); rc != 0)
        return Error::from_syscall("graphics_connector_get_head_mode_setting"sv, rc);
    size_t size = static_cast<size_t>(mode.horizontal_stride) * mode.vertical_active;
    auto* framebuffer = static_cast<u8*>(TRY(Core::System::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)));
    auto saved = TRY(ByteBuffer::create_uninitialized(size));
    auto pattern = TRY(ByteBuffer::create_uninitialized(size));
    for (size_t i = 0; i < size; ++i)
        pattern[i] = static_cast<u8>(i >> 4);

    say("Framebuffer {}: {}x{}, pitch {} ({} KiB). The screen is restored afterwards.", path,
        mode.horizontal_active, mode.vertical_active, mode.horizontal_stride, size / 1024);
    s_memcpy(saved.data(), framebuffer, size);
    report("  read, memcpy to RAM"sv, size, [&] { s_memcpy(pattern.data(), framebuffer, size); });
    report("  read, 32-bit loads"sv, size, [&] { (void)sum_words_volatile(framebuffer, size); });
    report("  write, memcpy from RAM"sv, size, [&] { s_memcpy(framebuffer, pattern.data(), size); });
    report("  write, memset"sv, size, [&] { s_memset(framebuffer, 0x40, size); });
    report("  write, 32-bit stores"sv, size, [&] { fill_words_volatile(framebuffer, size, 0x00808080); });
    report("  write, 8-bit stores"sv, size, [&] { fill_bytes_volatile(framebuffer, size, 0x20); });
#if ARCH(POWERPC32)
    report("  write, 64-bit FP copy from RAM"sv, size, [&] { copy_doubles(framebuffer, pattern.data(), size); });
#endif
    s_memcpy(framebuffer, saved.data(), size);
    TRY(Core::System::munmap(framebuffer, size));
    TRY(Core::System::close(fd));
    return {};
}

static ErrorOr<void> benchmark_disk(StringView path, size_t mebibytes, size_t block_size)
{
    int fd = TRY(Core::System::open(path, O_RDONLY | O_CLOEXEC));
    auto buffer = TRY(ByteBuffer::create_uninitialized(block_size));
    size_t total = mebibytes * MiB;
    size_t done = 0;
    double start = now_seconds();
    while (done < total) {
        auto count = TRY(Core::System::read(fd, buffer.bytes()));
        if (count == 0)
            break;
        done += count;
    }
    double elapsed = now_seconds() - start;
    TRY(Core::System::close(fd));
    say("Disk {}: read {} KiB in {} KiB requests in {:.2} s: {:.2} MB/s", path, done / KiB, block_size / KiB, elapsed,
        elapsed > 0 ? static_cast<double>(done) / elapsed / 1e6 : 0.0);
    return {};
}

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    bool should_verify = false;
    bool skip_ram = false;
    size_t ram_kib = 4096;
    StringView framebuffer_path;
    StringView disk_path;
    size_t disk_mib = 32;
    size_t disk_block_kib = 128;

    Core::ArgsParser args_parser;
    args_parser.set_general_help("Measure memory, framebuffer and raw disk throughput.");
    args_parser.add_option(should_verify, "Check memcpy, memset and memmove for every small alignment and overlap", "verify", 0);
    args_parser.add_option(skip_ram, "Skip the RAM benchmarks", "no-ram", 0);
    args_parser.add_option(ram_kib, "RAM buffer size in KiB (default 4096)", "ram-kib", 0, "size");
    args_parser.add_option(framebuffer_path, "Benchmark a display connector, e.g. /dev/gpu/connector0", "framebuffer", 'f', "path");
    args_parser.add_option(disk_path, "Benchmark sequential reads from a block device, e.g. /dev/hda", "disk", 'd', "path");
    args_parser.add_option(disk_mib, "MiB to read from the disk (default 32)", "disk-mib", 0, "size");
    args_parser.add_option(disk_block_kib, "Disk read size in KiB (default 128)", "disk-block-kib", 0, "size");
    args_parser.parse(arguments);

    if (should_verify)
        return verify();
    if (!skip_ram) {
        TRY(benchmark_ram(64 * KiB));
        TRY(benchmark_ram(ram_kib * KiB));
    }
    if (!framebuffer_path.is_empty())
        TRY(benchmark_framebuffer(framebuffer_path));
    if (!disk_path.is_empty())
        TRY(benchmark_disk(disk_path, disk_mib, disk_block_kib * KiB));
    return 0;
}
