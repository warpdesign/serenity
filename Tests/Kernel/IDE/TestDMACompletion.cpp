/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Devices/Storage/IDE/DMACompletion.h>

using Kernel::ide_dma_has_completed;
using Kernel::ide_dma_has_failed;
using Kernel::ide_dma_irq_acknowledgement;
using Kernel::ide_dma_irq_belongs_to_transfer;
using Kernel::ide_dma_should_stop;
using Kernel::ide_dma_pci_command_allows_mastering;
using Kernel::ide_dma_pin_interrupt_is_masked;
using Kernel::ide_drive_ready_after_reset;

static_assert(ide_dma_pin_interrupt_is_masked(1 << 10));
static_assert(!ide_dma_pin_interrupt_is_masked(0));
static_assert(!ide_dma_has_completed(0x58, 0x21)); // DRQ, active, no INTRQ.
static_assert(!ide_dma_has_completed(0x50, 0x20)); // Idle without INTRQ is not completion.
static_assert(ide_dma_has_completed(0x50, 0x24)); // INTRQ, all PRDs consumed.
static_assert(!ide_dma_has_completed(0x50, 0x25)); // INTRQ before PRD exhaustion is incomplete.
static_assert(ide_dma_should_stop(0x21, true)); // ISR already acknowledged this transfer's IRQ.
static_assert(!ide_dma_should_stop(0x21, false)); // An active engine alone is not completion.
static_assert(ide_dma_should_stop(0x24, false)); // Poller saw the completion latch.
static_assert(!ide_dma_has_completed(0x50, 0x21, true)); // IRQ seen, PRDs still active.
static_assert(!ide_dma_irq_belongs_to_transfer(false, 0x24)); // Firmware-pending IRQ before DMA starts.
static_assert(ide_dma_irq_belongs_to_transfer(true, 0x24)); // Current DMA command's IRQ.
static_assert(!ide_dma_irq_belongs_to_transfer(true, 0x20)); // Another device's IRQ.
static_assert(ide_dma_has_completed(0x50, 0x20, true)); // IRQ handler acknowledged the latch.
static_assert(!ide_dma_has_completed(0x58, 0x20, true)); // Drive still requests data.
static_assert(!ide_dma_has_completed(0x50, 0x22, true)); // Bus-master error takes precedence.
static_assert(!ide_dma_has_failed(0x2f, 0x21)); // Transient ATA status before INTRQ is not final.
static_assert(ide_dma_has_failed(0x2f, 0x25)); // The same status after INTRQ is an error.
static_assert(ide_dma_has_failed(0x50, 0x22)); // Bus-master error is final immediately.
static_assert(ide_dma_irq_acknowledgement(0x27) == 0x25); // Keep error latched while acknowledging IRQ.
static_assert(ide_dma_pci_command_allows_mastering(0x85)); // I/O and bus master enabled.
static_assert(!ide_dma_pci_command_allows_mastering(0x81)); // I/O enabled, bus master disabled.
static_assert(!ide_dma_pci_command_allows_mastering(0x84)); // Bus master enabled, I/O decoder disabled.
static_assert(ide_drive_ready_after_reset(0x50));
static_assert(!ide_drive_ready_after_reset(0x58));
static_assert(!ide_drive_ready_after_reset(0x51));
static_assert(!ide_drive_ready_after_reset(0xff));
static_assert(!ide_drive_ready_after_reset(0x00));

int main(int argc, char**)
{
    if (ide_dma_has_completed(0x51, 0x24)) // Drive error.
        return 1;
    if (ide_dma_has_completed(0x50, 0x26)) // PCI bus-master error.
        return 1;
    if (argc > 1) {
        if (ide_dma_has_completed(0x58, 0x24)) // Data request still asserted.
            return 1;
        if (ide_dma_has_completed(0xd0, 0x24)) // Drive still busy.
            return 1;
    }
    return 0;
}
