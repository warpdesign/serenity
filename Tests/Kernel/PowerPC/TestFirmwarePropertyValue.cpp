/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/Loader/FirmwarePropertyValue.h>
#include <array>
#include <cstdio>
#include <string>

int main()
{
    // SmartFirmware reported the full Radeon ROM length after a 128-byte getprop request.
    std::array<u8, 512> rom;
    rom.fill(0xcd);
    for (size_t index = 0; index < 128; ++index)
        rom[index] = 0xab;

    std::string output;
    auto write = [&](char const* bytes, u32 size) { output.append(bytes, size); };
    Kernel::PowerPC::Loader::write_firmware_property_value(write, rom.data(), rom.size(), 128, 0x10000);

    std::string expected = "<";
    for (int index = 0; index < 32; ++index) {
        if (index != 0)
            expected += ' ';
        expected += "0xabababab";
    }
    expected += "> (0x00010000 bytes)";
    if (output != expected) {
        std::fprintf(stderr, "Oversized firmware property printed %zu bytes; expected %zu\n", output.size(), expected.size());
        return 1;
    }

    output.clear();
    u8 const text[] = { 't', 'r', 'u', 'e', 0 };
    Kernel::PowerPC::Loader::write_firmware_property_value(write, text, sizeof(text), sizeof(text), sizeof(text));
    if (output != "\"true\"")
        return 1;

    return 0;
}
