/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/Pegasos2/IDEIRQRouting.h>

using Kernel::PowerPC::Pegasos2::ide_channel_irq;
using Kernel::PowerPC::Pegasos2::ide_legacy_channel_ports;
using Kernel::PowerPC::Pegasos2::should_force_ide_legacy_mode;

static_assert(ide_channel_irq(0x1106, 0x0571, 0, 0, 12, 1, 0) == 14);
static_assert(ide_channel_irq(0x1106, 0x0571, 0, 0, 12, 1, 1) == 15);
static_assert(!ide_channel_irq(0x1106, 0x0571, 0, 0, 12, 1, 2).has_value());
static_assert(!ide_channel_irq(0x1106, 0x0571, 0, 0, 13, 1, 0).has_value());
static_assert(!ide_channel_irq(0x1106, 0x0571, 0, 0, 12, 0, 0).has_value());
static_assert(!ide_channel_irq(0x1106, 0x0571, 0, 1, 12, 1, 0).has_value());
static_assert(!ide_channel_irq(0x1106, 0x0571, 1, 0, 12, 1, 0).has_value());
static_assert(!ide_channel_irq(0x1106, 0x1234, 0, 0, 12, 1, 0).has_value());
static_assert(should_force_ide_legacy_mode(0x1106, 0x0571, 0, 0, 12, 1, 14, true));
static_assert(!should_force_ide_legacy_mode(0x1106, 0x0571, 0, 0, 12, 1, 14, false));
static_assert(!should_force_ide_legacy_mode(0x1106, 0x0571, 0, 0, 12, 1, 9, true));
static_assert(ide_legacy_channel_ports(0).command == 0x1f0);
static_assert(ide_legacy_channel_ports(0).control == 0x3f6);
static_assert(ide_legacy_channel_ports(1).command == 0x170);
static_assert(ide_legacy_channel_ports(1).control == 0x376);

int main()
{
    return 0;
}
