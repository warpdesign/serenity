/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

namespace Kernel::PowerPC::Pegasos2::VIAAC97Codec {

static constexpr unsigned primary_valid = 1u << 25;
static constexpr unsigned read_bit = 1u << 23;

constexpr unsigned read_command(unsigned register_offset)
{
    return primary_valid | read_bit | ((register_offset & 0x7eu) << 16);
}

constexpr unsigned write_command(unsigned register_offset, unsigned value)
{
    return primary_valid | ((register_offset & 0x7eu) << 16) | (value & 0xffffu);
}

}
