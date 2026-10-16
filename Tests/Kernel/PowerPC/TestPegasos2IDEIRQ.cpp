/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/Pegasos2/IDEIRQRouting.h>

using Kernel::PowerPC::Pegasos2::ide_channel_irq;

static_assert(ide_channel_irq(0x1106, 0x0571, 0, 0, 12, 1, 0) == 14);
static_assert(ide_channel_irq(0x1106, 0x0571, 0, 0, 12, 1, 1) == 15);
static_assert(!ide_channel_irq(0x1106, 0x0571, 0, 0, 12, 1, 2).has_value());
static_assert(!ide_channel_irq(0x1106, 0x0571, 0, 0, 13, 1, 0).has_value());
static_assert(!ide_channel_irq(0x1106, 0x0571, 0, 0, 12, 0, 0).has_value());
static_assert(!ide_channel_irq(0x1106, 0x0571, 0, 1, 12, 1, 0).has_value());
static_assert(!ide_channel_irq(0x1106, 0x0571, 1, 0, 12, 1, 0).has_value());
static_assert(!ide_channel_irq(0x1106, 0x1234, 0, 0, 12, 1, 0).has_value());

int main()
{
    return 0;
}
