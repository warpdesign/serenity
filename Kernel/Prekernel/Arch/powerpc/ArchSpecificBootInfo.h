/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Platform.h>
#include <AK/Types.h>
#include <Kernel/Arch/powerpc/Pegasos2/OpenFirmwarePCIRanges.h>
#include <Kernel/Arch/powerpc/Pegasos2/RTAS.h>
#include <Kernel/Memory/PhysicalAddress.h>

VALIDATE_IS_POWERPC32()

namespace Kernel {

struct ArchSpecificBootInfo {
    FlatPtr open_firmware_entry { 0 };
    FlatPtr hpt_virtual_address { 0 };
    PhysicalAddress hpt_physical_address { 0 };
    u32 hpt_group_count { 0 };
    u32 timebase_frequency_hz { 0 };
    u32 mv64361_registers_physical_base { 0 };
    u32 pci_host_bridge_count { 0 };
    PowerPCPCIHostBridgeInfo pci_host_bridges[POWERPC_PEGASOS2_PCI_HOST_BRIDGE_COUNT] {};
    PowerPCRtasInfo rtas {};
};

}
