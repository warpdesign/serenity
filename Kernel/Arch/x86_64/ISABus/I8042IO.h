/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/Types.h>
#include <Kernel/Arch/x86_64/IO.h>

namespace Kernel {

class I8042IO {
public:
    static ErrorOr<I8042IO> create() { return I8042IO {}; }

    u8 read(u8 port) { return IO::in8(port); }
    void write(u8 port, u8 data) { IO::out8(port, data); }
};

}
