/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/Loader/AvailableMemoryClaim.h>
#include <array>
#include <cstdio>

int main()
{
    // SmartFirmware's reported 512 MiB RAM contains a gap and a reserved upper region.
    std::array<BootMemoryRange, 2> smartfirmware { BootMemoryRange { 0, 0x01000000 },
        BootMemoryRange { 0x01800000, 0x0e400000 } };
    u32 address = 0;
    if (!Kernel::PowerPC::Loader::choose_claim_probe_address(smartfirmware.data(), smartfirmware.size(),
            0x007c9000, 4096, 4 * 1024 * 1024, address)
        || address != 0x01800000) {
        std::fprintf(stderr, "Wrong SmartFirmware claim candidate: %#x\n", address);
        return 1;
    }

    // QEMU's VOF advertises the loader tail inside its largest available range.
    std::array<BootMemoryRange, 3> qemu { BootMemoryRange { 0x00000d80, 0x00007280 },
        BootMemoryRange { 0x00010000, 0x001f0000 }, BootMemoryRange { 0x007c7606, 0x1f8389fa } };
    if (!Kernel::PowerPC::Loader::choose_claim_probe_address(qemu.data(), qemu.size(),
            0x007c9000, 4096, 4 * 1024 * 1024, address)
        || address != 0x00800000) {
        std::fprintf(stderr, "Wrong QEMU claim candidate: %#x\n", address);
        return 1;
    }

    if (Kernel::PowerPC::Loader::choose_claim_probe_address(smartfirmware.data(), smartfirmware.size(),
            0x0fc00000, 4096, 4 * 1024 * 1024, address)) {
        std::fprintf(stderr, "Claim candidate escaped the available ranges\n");
        return 1;
    }
    return 0;
}
