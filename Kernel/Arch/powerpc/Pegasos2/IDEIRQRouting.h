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

constexpr bool should_force_ide_legacy_mode(u16 vendor, u16 device, u16 domain, u8 bus, u8 slot, u8 function, u8 pci_line, bool dma_requested)
{
    return dma_requested && pci_line == 14
        && ide_channel_irq(vendor, device, domain, bus, slot, function, 0).has_value();
}

struct IDELegacyChannelPorts {
    u16 command;
    u16 control;
};

constexpr IDELegacyChannelPorts ide_legacy_channel_ports(u8 channel)
{
    return channel == 0 ? IDELegacyChannelPorts { 0x1f0, 0x3f6 } : IDELegacyChannelPorts { 0x170, 0x376 };
}

}
