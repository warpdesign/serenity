/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

namespace Kernel::PowerPC::Loader {

struct LoaderInput {
    enum class Action {
        Ignore,
        Add,
        Erase,
        Boot,
        Help,
        Exit,
    };

    static constexpr unsigned capacity = 192;
    char options[capacity + 1] {};
    unsigned length { 0 };

    constexpr Action consume(unsigned char character)
    {
        if (character == '\r' || character == '\n') {
            if (equals("exit"))
                return Action::Exit;
            if (equals("/?")) {
                length = 0;
                options[0] = '\0';
                return Action::Help;
            }
            return Action::Boot;
        }
        if (character == 3)
            return Action::Exit;
        if (character == '\b' || character == 127) {
            if (length == 0)
                return Action::Ignore;
            options[--length] = '\0';
            return Action::Erase;
        }
        if (character == '\t')
            character = ' ';
        if (character < ' ' || character > '~' || length == capacity)
            return Action::Ignore;
        options[length++] = static_cast<char>(character);
        options[length] = '\0';
        return Action::Add;
    }

private:
    constexpr bool equals(char const* text) const
    {
        unsigned index = 0;
        while (index < length && text[index] != '\0') {
            if (options[index] != text[index])
                return false;
            ++index;
        }
        return index == length && text[index] == '\0';
    }
};

}
