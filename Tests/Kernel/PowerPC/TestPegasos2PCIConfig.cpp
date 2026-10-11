/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/Pegasos2/HardwareAddresses.h>
#include <Kernel/Arch/powerpc/Pegasos2/PCIConfig.h>

volatile u32 powerpc_pegasos2_pci_config_debug;

extern "C" int powerpc_pegasos2_pci_config_test(u32 controller_base)
{
    using Kernel::PowerPC::Pegasos2::pci_config_address;
    using Kernel::PowerPC::Pegasos2::read_pci_config32;

    auto encoded = pci_config_address(2, 3, 4, 0x20);
    if (!encoded.has_value() || encoded.value() != 0x80021c20u
        || pci_config_address(0, 32, 0, 0).has_value()
        || pci_config_address(0, 0, 8, 0).has_value()
        || pci_config_address(0, 0, 0, 1).has_value()
        || pci_config_address(0, 0, 0, 0x100).has_value())
        return 0;

    if (controller_base == 0 || (controller_base & 0xffffu) != 0)
        return 0;
    auto* south_address = reinterpret_cast<volatile u32*>(controller_base + PEGASOS2_PCI_SOUTH_CONFIG_ADDRESS_OFFSET);
    auto* south_data = reinterpret_cast<volatile u32*>(controller_base + PEGASOS2_PCI_SOUTH_CONFIG_DATA_OFFSET);
    auto* agp_address = reinterpret_cast<volatile u32*>(controller_base + PEGASOS2_PCI_AGP_CONFIG_ADDRESS_OFFSET);
    auto* agp_data = reinterpret_cast<volatile u32*>(controller_base + PEGASOS2_PCI_AGP_CONFIG_DATA_OFFSET);
    auto south_id = read_pci_config32(south_address, south_data, 0, 0, 0, 0);
    auto agp_id = read_pci_config32(agp_address, agp_data, 0, 0, 0, 0);
    if (!south_id.has_value() || !agp_id.has_value())
        return 0;
    powerpc_pegasos2_pci_config_debug = south_id.value();
    return (south_id.value() & 0xffffu) == 0x11abu
        && (agp_id.value() & 0xffffu) == 0x11abu
        && south_id.value() == agp_id.value();
}
