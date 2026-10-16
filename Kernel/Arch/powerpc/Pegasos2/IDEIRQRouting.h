/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Optional.h>
#include <AK/Types.h>

namespace Kernel::PowerPC::Pegasos2 {

// The VT8231 routes its two IDE channels through the fixed ISA IRQs even
// when firmware advertises the function as PCI native mode. Linux's Pegasos 2
// fixup likewise identifies IRQ 14 as the primary channel's legacy route.
constexpr Optional<u8> ide_channel_irq(u16 vendor, u16 device, u16 domain, u8 bus, u8 slot, u8 function, u8 channel)
{
    if (vendor != 0x1106 || device != 0x0571 || domain != 0 || bus != 0 || slot != 12 || function != 1 || channel >= 2)
        return {};
    return static_cast<u8>(14 + channel);
}

}
