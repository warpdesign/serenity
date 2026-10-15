/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Types.h>
#include <Kernel/Arch/DebugOutput.h>
#include <Kernel/Arch/powerpc/Pegasos2/HardwareAddresses.h>
#include <Kernel/Arch/powerpc/Pegasos2/LegacyIO.h>

namespace Kernel {

// The Pegasos 2 boot handoff keeps DBAT2 as an identity map of the PCI I/O window.
static volatile u8* s_uart = reinterpret_cast<volatile u8*>(PEGASOS2_UART_PHYSICAL_BASE);

void PowerPC::Pegasos2::set_debug_uart_physical_address(u32 address)
{
    s_uart = reinterpret_cast<volatile u8*>(address);
}

void debug_output(char character)
{
    auto* uart = s_uart;
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
