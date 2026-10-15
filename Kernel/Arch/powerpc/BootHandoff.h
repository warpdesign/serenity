/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Optional.h>
#include <AK/Span.h>
#include <Kernel/Arch/powerpc/HashPageTable.h>
#include <Kernel/Boot/BootInfo.h>

namespace Kernel::PowerPC {

// Validate the firmware handoff against SDR1 and produce the mapped HPT view.
bool boot_hpt_from_boot_info(BootInfo const&, PowerPCHashPageTable&);
Optional<u32> boot_timebase_frequency(BootInfo const&);
Optional<u32> boot_mv64361_registers_base(BootInfo const&);
Optional<ReadonlySpan<PowerPCPCIHostBridgeInfo>> boot_pci_host_bridges(BootInfo const&);
Optional<u32> boot_legacy_io_base(BootInfo const&);
Optional<PowerPCRtasInfo> boot_rtas_info(BootInfo const&);
// Use after the loader's low-memory handoff arrays are no longer mapped.
Optional<PowerPCRtasInfo> boot_rtas_info_after_handoff(BootInfo const&);

}
