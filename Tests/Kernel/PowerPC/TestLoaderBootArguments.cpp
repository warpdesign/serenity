/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/Loader/BootArguments.h>
#include <Kernel/Arch/powerpc/Loader/LoaderHelp.h>
#include <Kernel/Arch/powerpc/Loader/LoaderInput.h>
#include <cstring>
#include <initializer_list>

using Kernel::PowerPC::Loader::boot_argument_present;

static_assert(boot_argument_present("serial_debug weakrng fullram", "weakrng"));
static_assert(boot_argument_present("serial_debug weakrng fullram", "fullram"));
static_assert(!boot_argument_present("serial_debug oldfullram", "fullram"));
static_assert(!boot_argument_present("serial_debug fullram=1", "fullram"));
static_assert(boot_argument_present("fullram /? nodma", "/?"));
static_assert(!boot_argument_present("fullram /?suffix nodma", "/?"));

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
    if (!boot_argument_present("\t/?  fullram\t", "/?"))
        return 1;
    for (auto const* option : { "/?", "fullram", "fixedio", "ofdump", "halt", "stop", "realmode", "claimram", "native", "weakrng", "nodma", "udma=N", "root=..." }) {
        if (!std::strstr(Kernel::PowerPC::Loader::loader_help, option))
            return 1;
    }
    if (!std::strstr(Kernel::PowerPC::Loader::loader_help, "exit returns to firmware"))
        return 1;
    Kernel::PowerPC::Loader::LoaderInput input;
    for (auto character : "fullraX\bm weakrng nodma\r") {
        auto action = input.consume(character);
        if (character == '\r' && action != Kernel::PowerPC::Loader::LoaderInput::Action::Boot)
            return 1;
    }
    if (std::strcmp(input.options, "fullram weakrng nodma") != 0)
        return 1;
    Kernel::PowerPC::Loader::LoaderInput exit_input;
    for (auto character : "exit")
        exit_input.consume(character);
    if (exit_input.consume('\r') != Kernel::PowerPC::Loader::LoaderInput::Action::Exit)
        return 1;
    Kernel::PowerPC::Loader::LoaderInput help_input;
    help_input.consume('/');
    help_input.consume('?');
    if (help_input.consume('\r') != Kernel::PowerPC::Loader::LoaderInput::Action::Help || help_input.length != 0)
        return 1;
    Kernel::PowerPC::Loader::LoaderInput bounded_input;
    for (unsigned index = 0; index < bounded_input.capacity; ++index) {
        if (bounded_input.consume('x') != Kernel::PowerPC::Loader::LoaderInput::Action::Add)
            return 1;
    }
    if (bounded_input.consume('x') != Kernel::PowerPC::Loader::LoaderInput::Action::Ignore
        || bounded_input.length != bounded_input.capacity || bounded_input.options[bounded_input.capacity] != '\0')
        return 1;
    return 0;
}
