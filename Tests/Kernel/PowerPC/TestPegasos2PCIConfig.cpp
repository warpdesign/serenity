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
    using Kernel::PowerPC::Pegasos2::pci_config_register_offsets;
    using Kernel::PowerPC::Pegasos2::read_pci_config8;
    using Kernel::PowerPC::Pegasos2::read_pci_config16;
    using Kernel::PowerPC::Pegasos2::read_pci_config32;
    using Kernel::PowerPC::Pegasos2::write_pci_config8;
    using Kernel::PowerPC::Pegasos2::write_pci_config16;
    using Kernel::PowerPC::Pegasos2::write_pci_config32;

    auto encoded = pci_config_address(2, 3, 4, 0x20);
    if (!encoded.has_value() || encoded.value() != 0x80021c20u
        || pci_config_address(0, 32, 0, 0).has_value()
        || pci_config_address(0, 0, 8, 0).has_value()
        || pci_config_address(0, 0, 0, 1).has_value()
        || pci_config_address(0, 0, 0, 0x100).has_value())
        return 0;

    if (controller_base == 0 || (controller_base & 0xffffu) != 0)
        return 0;
    auto south_offsets = pci_config_register_offsets(0);
    auto agp_offsets = pci_config_register_offsets(1);
    if (!south_offsets.has_value() || !agp_offsets.has_value() || pci_config_register_offsets(2).has_value()
        || south_offsets->address_offset != PEGASOS2_PCI_SOUTH_CONFIG_ADDRESS_OFFSET
        || south_offsets->data_offset != PEGASOS2_PCI_SOUTH_CONFIG_DATA_OFFSET
        || agp_offsets->address_offset != PEGASOS2_PCI_AGP_CONFIG_ADDRESS_OFFSET
        || agp_offsets->data_offset != PEGASOS2_PCI_AGP_CONFIG_DATA_OFFSET)
        return 0;
    auto* south_address = reinterpret_cast<volatile u32*>(controller_base + south_offsets->address_offset);
    auto* south_data = reinterpret_cast<volatile u32*>(controller_base + south_offsets->data_offset);
    auto* agp_address = reinterpret_cast<volatile u32*>(controller_base + agp_offsets->address_offset);
    auto* agp_data = reinterpret_cast<volatile u32*>(controller_base + agp_offsets->data_offset);
    auto south_id = read_pci_config32(south_address, south_data, 0, 0, 0, 0);
    auto agp_id = read_pci_config32(agp_address, agp_data, 0, 0, 0, 0);
    if (!south_id.has_value() || !agp_id.has_value())
        return 0;
    powerpc_pegasos2_pci_config_debug = south_id.value();
    auto vendor = read_pci_config16(south_address, south_data, 0, 0, 0, 0);
    auto device = read_pci_config16(south_address, south_data, 0, 0, 0, 2);
    if (!vendor.has_value() || !device.has_value() || vendor.value() != (south_id.value() & 0xffffu)
        || device.value() != (south_id.value() >> 16)
        || read_pci_config16(south_address, south_data, 0, 0, 0, 1).has_value()
        || read_pci_config8(south_address, south_data, 0, 0, 0, 0x100).has_value())
        return 0;
    for (u16 offset = 0; offset < 4; ++offset) {
        auto byte = read_pci_config8(south_address, south_data, 0, 0, 0, offset);
        if (!byte.has_value() || byte.value() != ((south_id.value() >> (offset * 8)) & 0xffu))
            return 0;
    }
    if ((south_id.value() & 0xffffu) != 0x11abu || agp_id.value() != south_id.value())
        return 0;

    // The emulated VIA IDE command register supports bus-master toggling.
    // Write zero to the adjacent write-one-to-clear status halfword and
    // restore the command register before continuing the boot probe.
    powerpc_pegasos2_pci_config_debug = 0x10u;
    auto ide_id = read_pci_config32(south_address, south_data, 0, 12, 1, 0);
    auto original = read_pci_config32(south_address, south_data, 0, 12, 1, 0x04);
    if (!ide_id.has_value() || ide_id.value() != 0x05711106u || !original.has_value())
        return 0;
    powerpc_pegasos2_pci_config_debug = 0x20u;
    u8 original_command_byte = original.value() & 0xffu;
    u8 changed_command_byte = original_command_byte ^ 0x04u;
    u16 original_command = original.value() & 0xffffu;
    u16 changed_command = original_command ^ 0x04u;

    bool wrote_byte = write_pci_config8(south_address, south_data, 0, 12, 1, 0x04, changed_command_byte);
    auto byte_after = read_pci_config8(south_address, south_data, 0, 12, 1, 0x04);
    bool restored_byte = write_pci_config8(south_address, south_data, 0, 12, 1, 0x04, original_command_byte);
    if (!wrote_byte || !byte_after.has_value() || byte_after.value() != changed_command_byte || !restored_byte) {
        powerpc_pegasos2_pci_config_debug = 0x20000000u
            | (static_cast<u32>(original_command_byte) << 16)
            | (static_cast<u32>(changed_command_byte) << 8)
            | (byte_after.has_value() ? byte_after.value() : 0xffu);
        return 0;
    }

    powerpc_pegasos2_pci_config_debug = 0x30u;
    bool wrote_halfword = write_pci_config16(south_address, south_data, 0, 12, 1, 0x04, changed_command);
    auto halfword_after = read_pci_config16(south_address, south_data, 0, 12, 1, 0x04);
    bool restored_halfword = write_pci_config16(south_address, south_data, 0, 12, 1, 0x04, original_command);
    if (!wrote_halfword || !halfword_after.has_value() || halfword_after.value() != changed_command || !restored_halfword)
        return 0;

    powerpc_pegasos2_pci_config_debug = 0x40u;
    bool wrote_word = write_pci_config32(south_address, south_data, 0, 12, 1, 0x04, changed_command);
    auto word_after = read_pci_config32(south_address, south_data, 0, 12, 1, 0x04);
    bool restored_word = write_pci_config32(south_address, south_data, 0, 12, 1, 0x04, original_command);
    auto final_word = read_pci_config32(south_address, south_data, 0, 12, 1, 0x04);
    return wrote_word && word_after.has_value() && word_after.value() == ((original.value() & 0xffff0000u) | changed_command)
        && restored_word && final_word.has_value() && final_word.value() == original.value()
        && !write_pci_config8(south_address, south_data, 0, 12, 1, 0x100, 0)
        && !write_pci_config16(south_address, south_data, 0, 12, 1, 0x05, 0)
        && !write_pci_config32(south_address, south_data, 0, 12, 1, 0x05, 0);
}
