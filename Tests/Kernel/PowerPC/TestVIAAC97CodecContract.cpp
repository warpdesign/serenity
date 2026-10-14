/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/Pegasos2/VIAAC97Codec.h>

using namespace Kernel::PowerPC::Pegasos2;

static_assert(VIAAC97Codec::read_command(0x2c) == 0x02ac0000u);
static_assert(VIAAC97Codec::write_command(0x2c, 44100) == 0x022cac44u);

int main(int argc, char**)
{
    if (argc == 1)
        return 0;
    if (VIAAC97Codec::read_command(0x2d) != VIAAC97Codec::read_command(0x2c))
        return 1;
    if (VIAAC97Codec::write_command(0x2c, 0x1ac44) != VIAAC97Codec::write_command(0x2c, 0xac44))
        return 1;
    return 0;
}
