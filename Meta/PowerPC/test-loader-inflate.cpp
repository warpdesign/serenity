/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

// Runs the PowerPC loader's freestanding gzip decoder on the host, to debug it without QEMU.
//   clang++ -std=c++23 -O2 -I. Meta/PowerPC/test-loader-inflate.cpp \
//       Kernel/Arch/powerpc/Loader/Inflate.cpp -o /tmp/test-loader-inflate
//   /tmp/test-loader-inflate Build/powerpc/Kernel/Arch/powerpc/Loader/image/kernel.elf.gz Build/powerpc/Kernel/Kernel

#include <Kernel/Arch/powerpc/Loader/Inflate.h>
#include <chrono>
#include <cstdio>
#include <vector>

static std::vector<unsigned char> read_file(char const* path)
{
    std::vector<unsigned char> data;
    FILE* file = fopen(path, "rb");
    if (!file)
        return data;
    int byte;
    while ((byte = fgetc(file)) != EOF)
        data.push_back(static_cast<unsigned char>(byte));
    fclose(file);
    return data;
}

int main(int argc, char** argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s KERNEL.elf.gz EXPECTED_KERNEL\n", argv[0]);
        return 2;
    }
    auto compressed = read_file(argv[1]);
    auto expected = read_file(argv[2]);
    unsigned size = Kernel::PowerPC::Loader::gzip_uncompressed_size(compressed.data(), compressed.size());
    std::vector<unsigned char> output(size);
    auto start = std::chrono::steady_clock::now();
    bool inflated = Kernel::PowerPC::Loader::gzip_inflate(compressed.data(), compressed.size(), output.data(), size);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    bool matches = inflated && output == expected;
    printf("%zu -> %u bytes in %lld ms: %s\n", compressed.size(), size, static_cast<long long>(elapsed),
        matches ? "identical" : inflated ? "DIFFERENT" : "FAILED");

    // A flipped byte in the middle of the stream must be caught by the CRC or the decoder.
    compressed[compressed.size() / 2] ^= 0x40;
    bool damaged_accepted = Kernel::PowerPC::Loader::gzip_inflate(compressed.data(), compressed.size(), output.data(), size);
    printf("damaged stream %s\n", damaged_accepted ? "ACCEPTED" : "rejected");
    return matches && !damaged_accepted ? 0 : 1;
}
