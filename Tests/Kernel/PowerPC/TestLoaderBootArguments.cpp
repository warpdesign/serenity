/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/Loader/BootArguments.h>

using Kernel::PowerPC::Loader::boot_argument_present;

static_assert(boot_argument_present("serial_debug weakrng fullram", "weakrng"));
static_assert(boot_argument_present("serial_debug weakrng fullram", "fullram"));
static_assert(!boot_argument_present("serial_debug oldfullram", "fullram"));
static_assert(!boot_argument_present("serial_debug fullram=1", "fullram"));

int main()
{
    if (!boot_argument_present("\tweakrng  fullram\t", "weakrng"))
        return 1;
    if (!boot_argument_present("\tweakrng  fullram\t", "fullram"))
        return 1;
    if (boot_argument_present("insecure_entropy serenity-loader-use-installed-memory", "weakrng"))
        return 1;
    if (boot_argument_present("insecure_entropy serenity-loader-use-installed-memory", "fullram"))
        return 1;
    if (boot_argument_present("halted", "halt") || boot_argument_present("nativeboot", "native"))
        return 1;
    return 0;
}
