/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>

namespace Kernel {

constexpr bool ide_dma_pin_interrupt_is_masked(u16 pci_command)
{
    return (pci_command & (1 << 10)) != 0;
}

// PCI IDE bus-master status bit 2 is set by a rising edge on the drive's
// INTRQ line. Active alone means the command is still in progress.
constexpr bool ide_dma_has_completed(u8 drive_status, u8 bus_master_status)
{
    constexpr u8 busy_or_data_request_or_error = 0x80 | 0x08 | 0x20 | 0x01;
    constexpr u8 bus_master_interrupt = 0x04;
    constexpr u8 bus_master_error = 0x02;
    return (bus_master_status & bus_master_interrupt)
        && !(bus_master_status & bus_master_error)
        && !(drive_status & busy_or_data_request_or_error);
}

constexpr bool ide_drive_ready_after_reset(u8 status)
{
    constexpr u8 busy_or_data_request_or_error = 0x80 | 0x08 | 0x20 | 0x01;
    return (status & 0x40) && !(status & busy_or_data_request_or_error);
}

}
