/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/DebugOutput.h>
#include <Kernel/Arch/powerpc/CPU.h>

extern "C" void powerpc_enable_boot_translation(void);
extern "C" void powerpc_disable_boot_translation(void);

static void write_message(char const* message)
{
    for (; *message; ++message)
        Kernel::debug_output(*message);
}

extern "C" int powerpc_debug_output_test()
{
    if ((Kernel::PowerPC::read_msr() & 0x30u) != 0)
        return 0;
    write_message("SerenityOS PowerPC: real kernel debug UART passed\n");
    powerpc_enable_boot_translation();
    write_message("SerenityOS PowerPC: translated kernel debug UART passed\n");
    powerpc_disable_boot_translation();
    return (Kernel::PowerPC::read_msr() & 0x30u) == 0;
}
