/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Types.h>
#include <Kernel/Arch/DebugOutput.h>
#include <Kernel/Arch/powerpc/Pegasos2/HardwareAddresses.h>

namespace Kernel {

void debug_output(char character)
{
    // The Pegasos 2 boot handoff must keep DBAT2 as an identity map of the
    // PCI I/O window until a permanent MMIO mapping is installed.
    auto* uart = reinterpret_cast<volatile u8*>(PEGASOS2_UART_PHYSICAL_BASE);
    static bool initialized;
    static bool previous_was_carriage_return;
    auto write_register = [&](u32 index, u8 value) {
        uart[index] = value;
        asm volatile("eieio" : : : "memory");
    };

    if (!initialized) {
        write_register(1, 0);
        write_register(3, 0x80);
        write_register(0, 1);
        write_register(1, 0);
        write_register(3, 3);
        write_register(2, 7);
        write_register(4, 3);
        initialized = true;
    }

    auto write_character = [&](u8 value) {
        u32 attempts = 100'000;
        while ((uart[5] & 0x20u) == 0 && --attempts != 0)
            asm volatile("nop" : : : "memory");
        if (attempts != 0)
            write_register(0, value);
    };
    if (character == '\n' && !previous_was_carriage_return)
        write_character('\r');
    write_character(static_cast<u8>(character));
    previous_was_carriage_return = character == '\r';
}

}
