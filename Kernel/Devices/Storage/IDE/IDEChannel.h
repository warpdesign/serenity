/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/AtomicRefCounted.h>
#include <AK/Error.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/Optional.h>
#include <AK/Span.h>
#include <Kernel/Library/IOWindow.h>
#include <Kernel/Locking/Mutex.h>

namespace Kernel {

// One ATA channel (primary or secondary) of a PCI IDE controller, driven with polled PIO.
class IDEChannel final : public AtomicRefCounted<IDEChannel> {
public:
    enum class Drive : u8 {
        Master = 0,
        Slave = 1,
    };

    struct Identity {
        u64 sector_count { 0 };
        bool supports_lba48 { false };
    };

    static constexpr size_t sector_size = 512;

    static ErrorOr<NonnullRefPtr<IDEChannel>> create(NonnullOwnPtr<IOWindow> command_block, NonnullOwnPtr<IOWindow> control_block);

    ErrorOr<Optional<Identity>> identify(Drive);
    ErrorOr<void> read_sectors(Drive, bool use_lba48, u64 lba, size_t count, Bytes);
    ErrorOr<void> write_sectors(Drive, bool use_lba48, u64 lba, size_t count, ReadonlyBytes);

private:
    IDEChannel(NonnullOwnPtr<IOWindow> command_block, NonnullOwnPtr<IOWindow> control_block);

    u8 alternate_status();
    void select_drive(Drive, u8 lba_high_nibble);
    ErrorOr<u8> wait_until_not_busy();
    ErrorOr<void> wait_for_data_request();
    ErrorOr<void> issue_transfer_command(Drive, bool use_lba48, u64 lba, size_t count, u8 command_28, u8 command_48);
    ErrorOr<void> flush_cache(Drive, bool use_lba48);

    NonnullOwnPtr<IOWindow> m_command_block;
    NonnullOwnPtr<IOWindow> m_control_block;
    Mutex m_lock { "IDEChannel"sv };
};

}
