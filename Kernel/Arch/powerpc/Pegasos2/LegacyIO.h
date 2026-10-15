/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>
#include <Kernel/Boot/BootInfo.h>
#include <Kernel/Memory/PhysicalAddress.h>

namespace Kernel::PowerPC::Pegasos2 {

// Addresses come from the firmware handoff, falling back to the QEMU Pegasos 2 layout when it lacks them.
PhysicalAddress legacy_io_physical_base();
PhysicalAddress interrupt_acknowledge_physical_address();

// Must run before the first debug output; the loader keeps a DBAT identity map of the I/O window.
void configure_early_debug_uart(BootInfo const&);
void set_debug_uart_physical_address(u32);

}
