/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Array.h>
#include <AK/AtomicRefCounted.h>
#include <AK/Error.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/Optional.h>
#include <AK/OwnPtr.h>
#include <AK/Span.h>
#include <Kernel/Library/IOWindow.h>
#include <Kernel/Locking/Mutex.h>
#include <Kernel/Memory/PhysicalAddress.h>
#include <Kernel/Memory/Region.h>

namespace Kernel {

// One ATA channel (primary or secondary) of a PCI IDE controller. Transfers use bus-master
// DMA when the controller and drive allow it, otherwise PIO; completion is polled.
class IDEChannel final : public AtomicRefCounted<IDEChannel> {
public:
    enum class Drive : u8 {
        Master = 0,
        Slave = 1,
    };

    struct Identity {
        u64 sector_count { 0 };
        bool supports_lba48 { false };
        bool supports_dma { false };
        // Ultra DMA mode bit masks from IDENTIFY word 88, when that word is valid.
        u8 supported_udma_modes { 0 };
        u8 selected_udma_modes { 0 };
    };

    // The DMA buffer passed to transfers must be physically contiguous and at most this large.
    static constexpr size_t max_dma_transfer_size = 64 * KiB;
    static constexpr size_t sector_size = 512;

    static ErrorOr<NonnullRefPtr<IDEChannel>> create(NonnullOwnPtr<IOWindow> command_block, NonnullOwnPtr<IOWindow> control_block, OwnPtr<IOWindow> bus_master);

    ErrorOr<Optional<Identity>> identify(Drive);
    bool has_bus_master() const { return m_bus_master; }
    bool bus_master_reports_dma_capable(Drive) const;
    ErrorOr<void> enable_udma(Drive, u8 mode);
    bool is_dma_enabled(Drive drive) const { return m_dma_enabled[to_underlying(drive)]; }

    // physical_buffer is the physical address of buffer, used only for DMA.
    ErrorOr<void> read_sectors(Drive, bool use_lba48, u64 lba, size_t count, Bytes, PhysicalAddress physical_buffer);
    ErrorOr<void> write_sectors(Drive, bool use_lba48, u64 lba, size_t count, ReadonlyBytes, PhysicalAddress physical_buffer);

private:
    IDEChannel(NonnullOwnPtr<IOWindow> command_block, NonnullOwnPtr<IOWindow> control_block, OwnPtr<IOWindow> bus_master, OwnPtr<Memory::Region> descriptor_table, PhysicalAddress descriptor_table_physical);

    u8 alternate_status();
    void select_drive(Drive, u8 lba_high_nibble);
    ErrorOr<u8> wait_until_not_busy();
    ErrorOr<void> wait_for_data_request();
    ErrorOr<void> issue_transfer_command(Drive, bool use_lba48, u64 lba, size_t count, u8 command_28, u8 command_48);
    ErrorOr<void> flush_cache(Drive, bool use_lba48);
    ErrorOr<void> read_sectors_pio(Drive, bool use_lba48, u64 lba, size_t count, Bytes);
    ErrorOr<void> write_sectors_pio(Drive, bool use_lba48, u64 lba, size_t count, ReadonlyBytes);
    ErrorOr<void> transfer_dma(Drive, bool use_lba48, u64 lba, size_t count, bool is_write, u8* buffer, PhysicalAddress physical_buffer);
    void disable_dma_after_error(Drive, Error);

    NonnullOwnPtr<IOWindow> m_command_block;
    NonnullOwnPtr<IOWindow> m_control_block;
    OwnPtr<IOWindow> m_bus_master;
    OwnPtr<Memory::Region> m_descriptor_table;
    PhysicalAddress m_descriptor_table_physical;
    Array<bool, 2> m_dma_enabled {};
    Mutex m_lock { "IDEChannel"sv };
};

}
