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

constexpr bool ide_dma_pci_command_allows_mastering(u16 pci_command)
{
    // PCI Command bits 0 and 2 enable I/O port decoding and bus mastering.
    return (pci_command & 0x05) == 0x05;
}

// PCI IDE bus-master status bit 2 is set by a rising edge on the drive's
// INTRQ line. Active alone means the command is still in progress.
constexpr bool ide_dma_has_completed(u8 drive_status, u8 bus_master_status, bool irq_observed = false)
{
    constexpr u8 busy_or_data_request_or_error = 0x80 | 0x08 | 0x20 | 0x01;
    constexpr u8 bus_master_interrupt = 0x04;
    constexpr u8 bus_master_error = 0x02;
    return ((bus_master_status & bus_master_interrupt) || irq_observed)
        && !(bus_master_status & bus_master_error)
        && !(drive_status & busy_or_data_request_or_error);
}

// An ATA status sampled while DMA is active is not final. Linux's BMDMA path
// checks the bus-master interrupt first and then examines the drive status.
constexpr bool ide_dma_has_failed(u8 drive_status, u8 bus_master_status, bool irq_observed = false)
{
    constexpr u8 bus_master_interrupt = 0x04;
    constexpr u8 bus_master_error = 0x02;
    constexpr u8 drive_busy = 0x80;
    constexpr u8 drive_error_or_fault = 0x20 | 0x01;
    return (bus_master_status & bus_master_error)
        || (((bus_master_status & bus_master_interrupt) || irq_observed)
            && !(drive_status & drive_busy) && (drive_status & drive_error_or_fault));
}

// The IRQ handler acknowledges completion without clearing a simultaneous
// bus-master error before the transfer loop can observe it.
constexpr u8 ide_dma_irq_acknowledgement(u8 bus_master_status)
{
    return (bus_master_status & ~0x02) | 0x04;
}

constexpr bool ide_drive_ready_after_reset(u8 status)
{
    constexpr u8 busy_or_data_request_or_error = 0x80 | 0x08 | 0x20 | 0x01;
    return (status & 0x40) && !(status & busy_or_data_request_or_error);
}

}
