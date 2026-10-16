/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

namespace Kernel::PowerPC::Loader {

constexpr bool boot_argument_present(char const* command_line, char const* argument)
{
    if (!command_line || !argument || argument[0] == '\0')
        return false;
    for (unsigned offset = 0; command_line[offset] != '\0';) {
        while (command_line[offset] == ' ' || command_line[offset] == '\t')
            ++offset;
        if (command_line[offset] == '\0')
            break;
        unsigned index = 0;
        bool matches = true;
        while (command_line[offset] != '\0' && command_line[offset] != ' ' && command_line[offset] != '\t') {
            if (argument[index] == '\0' || command_line[offset] != argument[index])
                matches = false;
            else
                ++index;
            ++offset;
        }
        if (matches && argument[index] == '\0')
            return true;
    }
    return false;
}

constexpr bool boot_argument_with_prefix_present(char const* command_line, char const* prefix)
{
    if (!command_line || !prefix || prefix[0] == '\0')
        return false;
    for (unsigned offset = 0; command_line[offset] != '\0';) {
        while (command_line[offset] == ' ' || command_line[offset] == '\t')
            ++offset;
        if (command_line[offset] == '\0')
            break;
        unsigned index = 0;
        while (prefix[index] != '\0' && command_line[offset + index] == prefix[index])
            ++index;
        if (prefix[index] == '\0')
            return true;
        while (command_line[offset] != '\0' && command_line[offset] != ' ' && command_line[offset] != '\t')
            ++offset;
    }
    return false;
}

constexpr bool should_append_native_nodma(char const* command_line, bool is_qemu)
{
    return !is_qemu
        && !boot_argument_present(command_line, "nodma")
        && !boot_argument_present(command_line, "dma")
        && !boot_argument_with_prefix_present(command_line, "udma=");
}

}
