/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/StringBuilder.h>
#include <AK/Endian.h>
#include <Kernel/Arch/Delay.h>
#include <Kernel/Arch/MemoryFences.h>
#include <Kernel/Arch/Processor.h>
#include <Kernel/Devices/Storage/AHCI/Definitions.h>
#include <Kernel/Devices/Storage/IDE/IDEChannel.h>
#include <Kernel/Library/Assertions.h>
#include <Kernel/Memory/MemoryManager.h>
#include <Kernel/Time/TimeManagement.h>

namespace Kernel {

static constexpr u8 device_control_disable_interrupts = 1 << 1;
static constexpr u8 device_control_software_reset = 1 << 2;
static constexpr u8 device_select_lba_mode = 0xe0;
static constexpr size_t status_poll_attempts = 500'000;
static constexpr size_t status_poll_delay_us = 10;
static constexpr size_t max_sectors_per_lba28_command = 256;
static constexpr size_t max_sectors_per_lba48_command = 65536;
static constexpr size_t words_per_sector = IDEChannel::sector_size / sizeof(u16);

// PCI IDE Bus Master Programming Interface, revision 1.0.
static constexpr u8 bus_master_command = 0;
static constexpr u8 bus_master_status = 2;
static constexpr u8 bus_master_descriptor_table = 4;
static constexpr u8 bus_master_command_start = 1 << 0;
static constexpr u8 bus_master_command_write_to_memory = 1 << 3;
static constexpr u8 bus_master_status_active = 1 << 0;
static constexpr u8 bus_master_status_error = 1 << 1;
static constexpr u8 bus_master_status_interrupt = 1 << 2;
static constexpr u8 bus_master_status_drive_dma_capable(IDEChannel::Drive drive) { return 1 << (5 + to_underlying(drive)); }

static constexpr u8 ata_command_set_features = 0xef;
static constexpr u8 set_features_transfer_mode = 0x03;
static constexpr u8 transfer_mode_udma = 0x40;

struct [[gnu::packed]] PhysicalRegionDescriptor {
    LittleEndian<u32> address;
    LittleEndian<u16> byte_count;
    LittleEndian<u16> flags;
};
static constexpr u16 descriptor_end_of_table = 0x8000;
// A descriptor may not cross a 64 KiB boundary, so a 64 KiB buffer needs at most two.
static constexpr size_t max_descriptors = 2;

ErrorOr<NonnullRefPtr<IDEChannel>> IDEChannel::create(NonnullOwnPtr<IOWindow> command_block, NonnullOwnPtr<IOWindow> control_block, OwnPtr<IOWindow> bus_master)
{
    OwnPtr<Memory::Region> descriptor_table;
    PhysicalAddress descriptor_table_physical;
    if (bus_master) {
        RefPtr<Memory::PhysicalRAMPage> page;
        auto table_or_error = MM.allocate_dma_buffer_page("IDE PRD table"sv, Memory::Region::Access::ReadWrite, page, Memory::MemoryType::NonCacheable);
        if (table_or_error.is_error()) {
            dmesgln("IDE: Using PIO, the DMA descriptor table could not be allocated: {}", table_or_error.error());
            bus_master = nullptr;
        } else {
            descriptor_table = table_or_error.release_value();
            descriptor_table_physical = page->paddr();
            dmesgln("IDE: DMA descriptor table at {}", descriptor_table_physical);
        }
    }
    return adopt_nonnull_ref_or_enomem(new (nothrow) IDEChannel(move(command_block), move(control_block), move(bus_master), move(descriptor_table), descriptor_table_physical));
}

IDEChannel::IDEChannel(NonnullOwnPtr<IOWindow> command_block, NonnullOwnPtr<IOWindow> control_block, OwnPtr<IOWindow> bus_master, OwnPtr<Memory::Region> descriptor_table, PhysicalAddress descriptor_table_physical)
    : m_command_block(move(command_block))
    , m_control_block(move(control_block))
    , m_bus_master(move(bus_master))
    , m_descriptor_table(move(descriptor_table))
    , m_descriptor_table_physical(descriptor_table_physical)
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

    constexpr u16 dma_supported = 1 << 8;
    constexpr u16 word_88_valid = 1 << 2;
    Identity identity;
    identity.supports_lba48 = (words[83] & lba48_supported) != 0;
    identity.supports_dma = (words[49] & dma_supported) != 0;
    if (words[53] & word_88_valid) {
        identity.supported_udma_modes = words[88] & 0x7f;
        identity.selected_udma_modes = (words[88] >> 8) & 0x7f;
    }
    if (identity.supports_lba48) {
        identity.sector_count = static_cast<u64>(words[100]) | (static_cast<u64>(words[101]) << 16)
            | (static_cast<u64>(words[102]) << 32) | (static_cast<u64>(words[103]) << 48);
    } else {
        identity.sector_count = static_cast<u64>(words[60]) | (static_cast<u64>(words[61]) << 16);
    }
    dmesgln("IDE: {} drive \"{}\", {} sectors, LBA{}, DMA {}, UDMA modes supported {:#02x} selected {:#02x}, MWDMA word {:#04x}",
        drive == Drive::Master ? "Master"sv : "Slave"sv, model_name, identity.sector_count, identity.supports_lba48 ? 48 : 28,
        identity.supports_dma ? "yes"sv : "no"sv, identity.supported_udma_modes, identity.selected_udma_modes, words[63]);
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

bool IDEChannel::bus_master_reports_dma_capable(Drive drive) const
{
    return m_bus_master && (m_bus_master->read8(bus_master_status) & bus_master_status_drive_dma_capable(drive));
}

ErrorOr<void> IDEChannel::enable_udma(Drive drive, u8 mode)
{
    VERIFY(mode <= 6);
    if (!m_bus_master)
        return Error::from_errno(ENOTSUP);
    MutexLocker locker(m_lock);
    TRY(wait_until_not_busy());
    select_drive(drive, 0);
    m_command_block->write8(ATA_REG_FEATURES, set_features_transfer_mode);
    m_command_block->write8(ATA_REG_SECCOUNT0, transfer_mode_udma | mode);
    m_command_block->write8(ATA_REG_COMMAND, ata_command_set_features);
    u8 status = TRY(wait_until_not_busy());
    if (status & (ATA_SR_ERR | ATA_SR_DF))
        return Error::from_errno(EIO);
    // Tell the controller this drive can do DMA; BIOSes set these bits for the drives they configured.
    u8 bus_master_status_value = m_bus_master->read8(bus_master_status) & ~(bus_master_status_error | bus_master_status_interrupt);
    m_bus_master->write8(bus_master_status, bus_master_status_value | bus_master_status_drive_dma_capable(drive));
    m_dma_enabled[to_underlying(drive)] = true;
    return {};
}

void IDEChannel::disable_dma_after_error(Drive drive, Error error)
{
    dmesgln("IDE: {} drive DMA transfer failed ({}), using PIO from now on", drive == Drive::Master ? "Master"sv : "Slave"sv, error);
    m_dma_enabled[to_underlying(drive)] = false;
}

ErrorOr<void> IDEChannel::reset_after_dma_error()
{
    VERIFY(m_lock.is_locked());
    // SRST applies to both drives on this channel and may clear either
    // drive's negotiated transfer mode.
    m_dma_enabled.fill(false);
    // Stopping the bus master does not abort the command in the drive. A
    // timed-out DMA command can leave DRQ asserted, so reset before issuing
    // the replacement PIO command.
    m_control_block->write8(ATA_CTL_CONTROL, device_control_disable_interrupts);
    microseconds_delay(20);
    m_control_block->write8(ATA_CTL_CONTROL, device_control_disable_interrupts | device_control_software_reset);
    microseconds_delay(20);
    m_control_block->write8(ATA_CTL_CONTROL, device_control_disable_interrupts);
    // libata waits before reading taskfile status after SRST; some devices
    // transiently report ready before their reset actually starts.
    microseconds_delay(150'000);

    for (size_t attempt = 0; attempt < status_poll_attempts; ++attempt) {
        u8 status = alternate_status();
        if (!(status & (ATA_SR_BSY | ATA_SR_DRQ))) {
            if (status & (ATA_SR_ERR | ATA_SR_DF))
                return Error::from_errno(EIO);
            dmesgln("IDE: Channel reset after DMA failure, drive status {:#02x}", status);
            return {};
        }
        microseconds_delay(status_poll_delay_us);
    }
    dmesgln("IDE: Channel reset after DMA failure timed out, drive status {:#02x}", alternate_status());
    return Error::from_errno(ETIMEDOUT);
}

ErrorOr<void> IDEChannel::transfer_dma(Drive drive, bool use_lba48, u64 lba, size_t count, bool is_write, u8* buffer, PhysicalAddress physical_buffer)
{
    VERIFY(m_lock.is_locked());
    size_t size = count * sector_size;
    VERIFY(size > 0 && size <= max_dma_transfer_size);

    auto* descriptors = reinterpret_cast<PhysicalRegionDescriptor*>(m_descriptor_table->vaddr().as_ptr());
    size_t descriptor_count = 0;
    u64 address = physical_buffer.get();
    for (size_t remaining = size; remaining > 0;) {
        VERIFY(descriptor_count < max_descriptors && address + remaining <= 0x1'0000'0000ull);
        size_t length = min<size_t>(remaining, 0x10000 - (address & 0xffff));
        descriptors[descriptor_count].address = static_cast<u32>(address);
        // A byte count of zero means 64 KiB.
        descriptors[descriptor_count].byte_count = static_cast<u16>(length & 0xffff);
        descriptors[descriptor_count].flags = 0;
        ++descriptor_count;
        address += length;
        remaining -= length;
    }
    descriptors[descriptor_count - 1].flags = descriptor_end_of_table;

    // Write back data for the drive, and drop lines that would otherwise hide what the drive writes.
    Processor::flush_data_cache(VirtualAddress(buffer), size);
    full_memory_fence();

    u8 direction = is_write ? 0 : bus_master_command_write_to_memory;
    m_bus_master->write8(bus_master_command, direction);
    m_bus_master->write8(bus_master_status, m_bus_master->read8(bus_master_status) | bus_master_status_error | bus_master_status_interrupt);
    m_bus_master->write32(bus_master_descriptor_table, m_descriptor_table_physical.get());
    TRY(issue_transfer_command(drive, use_lba48, lba, count, is_write ? ATA_CMD_WRITE_DMA : ATA_CMD_READ_DMA, is_write ? ATA_CMD_WRITE_DMA_EXT : ATA_CMD_READ_DMA_EXT));
    m_bus_master->write8(bus_master_command, direction | bus_master_command_start);

    // Interrupts are disabled at the drive, so wait for both the drive and the DMA engine to go idle.
    ErrorOr<void> result = Error::from_errno(ETIMEDOUT);
    u8 drive_status = 0;
    u8 engine_status = 0;
    for (size_t attempt = 0; attempt < status_poll_attempts; ++attempt) {
        drive_status = alternate_status();
        engine_status = m_bus_master->read8(bus_master_status);
        if (engine_status & bus_master_status_error) {
            result = Error::from_errno(EIO);
            break;
        }
        if (!(drive_status & (ATA_SR_BSY | ATA_SR_DRQ))) {
            if (drive_status & (ATA_SR_ERR | ATA_SR_DF)) {
                result = Error::from_errno(EIO);
                break;
            }
            if (!(engine_status & bus_master_status_active)) {
                result = {};
                break;
            }
        }
        microseconds_delay(status_poll_delay_us);
    }

    m_bus_master->write8(bus_master_command, direction);
    u8 drive_error = result.is_error() ? m_command_block->read8(ATA_REG_ERROR) : 0;
    m_bus_master->write8(bus_master_status, m_bus_master->read8(bus_master_status) | bus_master_status_error | bus_master_status_interrupt);
    // Reading the status register acknowledges the drive.
    (void)m_command_block->read8(ATA_REG_STATUS);
    if (!is_write)
        Processor::flush_data_cache(VirtualAddress(buffer), size);
    if (result.is_error()) {
        dmesgln("IDE: DMA {} of {} sectors at {} failed, drive status {:#02x}, drive error {:#02x}, DMA status {:#02x}", is_write ? "write"sv : "read"sv, count, lba, drive_status, drive_error, engine_status);
        dmesgln("IDE: DMA PRD at {}: buffer {:#08x}, bytes {}, flags {:#04x}", m_descriptor_table_physical,
            static_cast<u32>(descriptors[0].address), static_cast<u16>(descriptors[0].byte_count), static_cast<u16>(descriptors[0].flags));
    }
    return result;
}

ErrorOr<void> IDEChannel::read_sectors(Drive drive, bool use_lba48, u64 lba, size_t count, Bytes buffer, PhysicalAddress physical_buffer)
{
    VERIFY(buffer.size() >= count * sector_size);
    MutexLocker locker(m_lock);
    if (is_dma_enabled(drive)) {
        auto result = transfer_dma(drive, use_lba48, lba, count, false, buffer.data(), physical_buffer);
        if (!result.is_error())
            return {};
        disable_dma_after_error(drive, result.release_error());
        TRY(reset_after_dma_error());
    }
    return read_sectors_pio(drive, use_lba48, lba, count, buffer);
}

ErrorOr<void> IDEChannel::write_sectors(Drive drive, bool use_lba48, u64 lba, size_t count, ReadonlyBytes buffer, PhysicalAddress physical_buffer)
{
    VERIFY(buffer.size() >= count * sector_size);
    MutexLocker locker(m_lock);
    if (is_dma_enabled(drive)) {
        auto result = transfer_dma(drive, use_lba48, lba, count, true, const_cast<u8*>(buffer.data()), physical_buffer);
        if (!result.is_error()) {
            m_write_cache_state[to_underlying(drive)].write_completed();
            return {};
        }
        disable_dma_after_error(drive, result.release_error());
        TRY(reset_after_dma_error());
    }
    TRY(write_sectors_pio(drive, use_lba48, lba, count, buffer));
    m_write_cache_state[to_underlying(drive)].write_completed();
    return {};
}

ErrorOr<void> IDEChannel::flush_device_cache(Drive drive, bool use_lba48)
{
    MutexLocker locker(m_lock);
    auto& state = m_write_cache_state[to_underlying(drive)];
    if (!state.needs_flush())
        return {};
    auto writes = state.pending_writes();
    auto started = TimeManagement::the().current_time(CLOCK_MONOTONIC_COARSE);
    TRY(flush_cache(drive, use_lba48));
    auto elapsed_ms = (TimeManagement::the().current_time(CLOCK_MONOTONIC_COARSE) - started).to_milliseconds();
    if (writes >= 8 || elapsed_ms >= 100)
        dmesgln("IDE: cache flush after {} writes took {} ms", writes, elapsed_ms);
    state.flush_completed();
    return {};
}

ErrorOr<void> IDEChannel::read_sectors_pio(Drive drive, bool use_lba48, u64 lba, size_t count, Bytes buffer)
{
    VERIFY(m_lock.is_locked());

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

ErrorOr<void> IDEChannel::write_sectors_pio(Drive drive, bool use_lba48, u64 lba, size_t count, ReadonlyBytes buffer)
{
    VERIFY(m_lock.is_locked());

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
    return {};
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
