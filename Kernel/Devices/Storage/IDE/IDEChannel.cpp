/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/StringBuilder.h>
#include <Kernel/Arch/Delay.h>
#include <Kernel/Devices/Storage/AHCI/Definitions.h>
#include <Kernel/Devices/Storage/IDE/IDEChannel.h>
#include <Kernel/Library/Assertions.h>

namespace Kernel {

static constexpr u8 device_control_disable_interrupts = 1 << 1;
static constexpr u8 device_select_lba_mode = 0xe0;
static constexpr size_t status_poll_attempts = 500'000;
static constexpr size_t status_poll_delay_us = 10;
static constexpr size_t max_sectors_per_lba28_command = 256;
static constexpr size_t max_sectors_per_lba48_command = 65536;
static constexpr size_t words_per_sector = IDEChannel::sector_size / sizeof(u16);

ErrorOr<NonnullRefPtr<IDEChannel>> IDEChannel::create(NonnullOwnPtr<IOWindow> command_block, NonnullOwnPtr<IOWindow> control_block)
{
    return adopt_nonnull_ref_or_enomem(new (nothrow) IDEChannel(move(command_block), move(control_block)));
}

IDEChannel::IDEChannel(NonnullOwnPtr<IOWindow> command_block, NonnullOwnPtr<IOWindow> control_block)
    : m_command_block(move(command_block))
    , m_control_block(move(control_block))
{
    // Transfers are polled, so keep the drives from raising INTRQ.
    m_control_block->write8(ATA_CTL_CONTROL, device_control_disable_interrupts);
}

u8 IDEChannel::alternate_status()
{
    return m_control_block->read8(ATA_CTL_ALTSTATUS);
}

void IDEChannel::select_drive(Drive drive, u8 lba_high_nibble)
{
    m_command_block->write8(ATA_REG_HDDEVSEL, device_select_lba_mode | (to_underlying(drive) << 4) | (lba_high_nibble & 0x0f));
    // Reading the alternate status register four times gives the drive the 400ns it needs to respond.
    for (int i = 0; i < 4; ++i)
        (void)alternate_status();
}

ErrorOr<u8> IDEChannel::wait_until_not_busy()
{
    for (size_t attempt = 0; attempt < status_poll_attempts; ++attempt) {
        u8 status = alternate_status();
        if (!(status & ATA_SR_BSY))
            return status;
        microseconds_delay(status_poll_delay_us);
    }
    return Error::from_errno(ETIMEDOUT);
}

ErrorOr<void> IDEChannel::wait_for_data_request()
{
    u8 status = TRY(wait_until_not_busy());
    if (status & (ATA_SR_ERR | ATA_SR_DF))
        return Error::from_errno(EIO);
    if (!(status & ATA_SR_DRQ))
        return Error::from_errno(EIO);
    return {};
}

ErrorOr<Optional<IDEChannel::Identity>> IDEChannel::identify(Drive drive)
{
    MutexLocker locker(m_lock);

    select_drive(drive, 0);
    // A floating bus reads as all ones when no drive is attached.
    if (m_command_block->read8(ATA_REG_STATUS) == 0xff)
        return Optional<Identity> {};

    m_command_block->write8(ATA_REG_SECCOUNT0, 0);
    m_command_block->write8(ATA_REG_LBA0, 0);
    m_command_block->write8(ATA_REG_LBA1, 0);
    m_command_block->write8(ATA_REG_LBA2, 0);
    m_command_block->write8(ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    if (m_command_block->read8(ATA_REG_STATUS) == 0)
        return Optional<Identity> {};

    auto status_or_error = wait_until_not_busy();
    if (status_or_error.is_error())
        return Optional<Identity> {};
    // ATAPI and SATA devices abort IDENTIFY and leave their signature in the LBA registers.
    if (m_command_block->read8(ATA_REG_LBA1) != 0 || m_command_block->read8(ATA_REG_LBA2) != 0)
        return Optional<Identity> {};
    if (status_or_error.value() & ATA_SR_ERR)
        return Optional<Identity> {};
    TRY(wait_for_data_request());

    // IDENTIFY data is a sequence of little-endian words; read16() already yields host-order values.
    Array<u16, words_per_sector> words;
    for (auto& word : words)
        word = m_command_block->read16(ATA_REG_DATA);

    StringBuilder model;
    for (size_t i = 27; i <= 46; ++i) {
        model.append(static_cast<char>(words[i] >> 8));
        model.append(static_cast<char>(words[i] & 0xff));
    }
    auto model_name = model.string_view().trim_whitespace();

    constexpr u16 lba_supported = 1 << 9;
    constexpr u16 lba48_supported = 1 << 10;
    if (!(words[49] & lba_supported)) {
        dmesgln("IDE: Ignoring drive \"{}\" without LBA support", model_name);
        return Optional<Identity> {};
    }

    Identity identity;
    identity.supports_lba48 = (words[83] & lba48_supported) != 0;
    if (identity.supports_lba48) {
        identity.sector_count = static_cast<u64>(words[100]) | (static_cast<u64>(words[101]) << 16)
            | (static_cast<u64>(words[102]) << 32) | (static_cast<u64>(words[103]) << 48);
    } else {
        identity.sector_count = static_cast<u64>(words[60]) | (static_cast<u64>(words[61]) << 16);
    }
    dmesgln("IDE: {} drive \"{}\", {} sectors, LBA{}", drive == Drive::Master ? "Master"sv : "Slave"sv,
        model_name, identity.sector_count, identity.supports_lba48 ? 48 : 28);
    if (identity.sector_count == 0)
        return Optional<Identity> {};
    return identity;
}

ErrorOr<void> IDEChannel::issue_transfer_command(Drive drive, bool use_lba48, u64 lba, size_t count, u8 command_28, u8 command_48)
{
    TRY(wait_until_not_busy());
    if (use_lba48) {
        VERIFY(count > 0 && count <= max_sectors_per_lba48_command);
        select_drive(drive, 0);
        // LBA48 registers are two-deep FIFOs: write the high-order bytes first.
        m_command_block->write8(ATA_REG_SECCOUNT0, (count >> 8) & 0xff);
        m_command_block->write8(ATA_REG_LBA0, (lba >> 24) & 0xff);
        m_command_block->write8(ATA_REG_LBA1, (lba >> 32) & 0xff);
        m_command_block->write8(ATA_REG_LBA2, (lba >> 40) & 0xff);
        m_command_block->write8(ATA_REG_SECCOUNT0, count & 0xff);
        m_command_block->write8(ATA_REG_LBA0, lba & 0xff);
        m_command_block->write8(ATA_REG_LBA1, (lba >> 8) & 0xff);
        m_command_block->write8(ATA_REG_LBA2, (lba >> 16) & 0xff);
        m_command_block->write8(ATA_REG_COMMAND, command_48);
    } else {
        VERIFY(count > 0 && count <= max_sectors_per_lba28_command);
        VERIFY(lba + count <= (1ull << 28));
        select_drive(drive, (lba >> 24) & 0x0f);
        m_command_block->write8(ATA_REG_SECCOUNT0, count & 0xff);
        m_command_block->write8(ATA_REG_LBA0, lba & 0xff);
        m_command_block->write8(ATA_REG_LBA1, (lba >> 8) & 0xff);
        m_command_block->write8(ATA_REG_LBA2, (lba >> 16) & 0xff);
        m_command_block->write8(ATA_REG_COMMAND, command_28);
    }
    return {};
}

ErrorOr<void> IDEChannel::read_sectors(Drive drive, bool use_lba48, u64 lba, size_t count, Bytes buffer)
{
    VERIFY(buffer.size() >= count * sector_size);
    MutexLocker locker(m_lock);

    auto max_per_command = use_lba48 ? max_sectors_per_lba48_command : max_sectors_per_lba28_command;
    for (size_t done = 0; done < count;) {
        auto chunk = min(count - done, max_per_command);
        TRY(issue_transfer_command(drive, use_lba48, lba + done, chunk, ATA_CMD_READ_PIO, ATA_CMD_READ_PIO_EXT));
        for (size_t sector = 0; sector < chunk; ++sector) {
            TRY(wait_for_data_request());
            auto* destination = buffer.offset_pointer((done + sector) * sector_size);
            for (size_t i = 0; i < words_per_sector; ++i) {
                // Sector data is a byte stream; keep the drive's little-endian word layout.
                u16 word = m_command_block->read16(ATA_REG_DATA);
                destination[i * 2] = word & 0xff;
                destination[i * 2 + 1] = word >> 8;
            }
        }
        if (m_command_block->read8(ATA_REG_STATUS) & (ATA_SR_ERR | ATA_SR_DF))
            return Error::from_errno(EIO);
        done += chunk;
    }
    return {};
}

ErrorOr<void> IDEChannel::write_sectors(Drive drive, bool use_lba48, u64 lba, size_t count, ReadonlyBytes buffer)
{
    VERIFY(buffer.size() >= count * sector_size);
    MutexLocker locker(m_lock);

    auto max_per_command = use_lba48 ? max_sectors_per_lba48_command : max_sectors_per_lba28_command;
    for (size_t done = 0; done < count;) {
        auto chunk = min(count - done, max_per_command);
        TRY(issue_transfer_command(drive, use_lba48, lba + done, chunk, ATA_CMD_WRITE_PIO, ATA_CMD_WRITE_PIO_EXT));
        for (size_t sector = 0; sector < chunk; ++sector) {
            TRY(wait_for_data_request());
            auto const* source = buffer.offset_pointer((done + sector) * sector_size);
            for (size_t i = 0; i < words_per_sector; ++i)
                m_command_block->write16(ATA_REG_DATA, static_cast<u16>(source[i * 2] | (source[i * 2 + 1] << 8)));
        }
        u8 status = TRY(wait_until_not_busy());
        if (status & (ATA_SR_ERR | ATA_SR_DF))
            return Error::from_errno(EIO);
        done += chunk;
    }
    return flush_cache(drive, use_lba48);
}

ErrorOr<void> IDEChannel::flush_cache(Drive drive, bool use_lba48)
{
    VERIFY(m_lock.is_locked());
    TRY(wait_until_not_busy());
    select_drive(drive, 0);
    m_command_block->write8(ATA_REG_COMMAND, use_lba48 ? ATA_CMD_CACHE_FLUSH_EXT : ATA_CMD_CACHE_FLUSH);
    u8 status = TRY(wait_until_not_busy());
    if (status & (ATA_SR_ERR | ATA_SR_DF))
        return Error::from_errno(EIO);
    return {};
}

}
