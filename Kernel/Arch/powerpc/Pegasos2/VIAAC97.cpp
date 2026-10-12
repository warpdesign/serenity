/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Endian.h>
#include <AK/NumericLimits.h>
#include <Kernel/Arch/Delay.h>
#include <Kernel/Arch/MemoryFences.h>
#include <Kernel/Arch/powerpc/Pegasos2/VIAAC97.h>
#include <Kernel/Bus/PCI/API.h>
#include <Kernel/Bus/PCI/Access.h>
#include <Kernel/Devices/Audio/Management.h>
#include <Kernel/Library/IOWindow.h>
#include <Kernel/Locking/Spinlock.h>
#include <Kernel/Memory/MemoryManager.h>
#include <Kernel/Memory/Region.h>
#include <Kernel/Sections.h>
#include <Kernel/Tasks/Thread.h>

namespace Kernel::PowerPC::Pegasos2 {

static constexpr u8 playback_status_register = 0x00;
static constexpr u8 playback_control_register = 0x01;
static constexpr u8 playback_type_register = 0x02;
static constexpr u8 playback_table_register = 0x04;
static constexpr u8 codec_command_register = 0x80;
static constexpr u8 pci_ac_link_status_register = 0x40;
static constexpr u8 pci_ac_link_control_register = 0x41;

static constexpr u8 codec_reset_register = 0x00;
static constexpr u8 codec_master_volume_register = 0x02;
static constexpr u8 codec_pcm_volume_register = 0x18;
static constexpr u8 codec_extended_id_register = 0x28;
static constexpr u8 codec_extended_control_register = 0x2a;
static constexpr u8 codec_dac_rate_register = 0x2c;

static constexpr u8 playback_end_of_list = 1 << 1;
static constexpr u8 playback_stopped = 1 << 2;
static constexpr u8 playback_start = 1 << 7;
static constexpr u8 playback_terminate = 1 << 6;
static constexpr u8 playback_stereo_16bit = (1 << 5) | (1 << 4);

static constexpr u32 descriptor_end_of_list = 1u << 31;
static constexpr u32 codec_read_command = 1u << 23;
static constexpr u32 codec_data_valid = 1u << 25;
static constexpr u32 codec_busy = 1u << 24;

UNMAP_AFTER_INIT ErrorOr<bool> VIAAC97::probe(PCI::DeviceIdentifier const& identifier)
{
    if (identifier.hardware_id() != PCI::HardwareID { 0x1106, 0x3058 })
        return false;
    if (PCI::get_BAR_space_size(identifier, PCI::HeaderType0BaseRegister::BAR0) < 0x100)
        return Error::from_errno(EIO);
    return PCI::get_BAR_space_type(PCI::get_BAR(identifier, PCI::HeaderType0BaseRegister::BAR0)) == PCI::BARSpaceType::IOSpace;
}

UNMAP_AFTER_INIT ErrorOr<NonnullRefPtr<AudioController>> VIAAC97::create(PCI::DeviceIdentifier const& identifier)
{
    auto io_window = TRY(IOWindow::create_for_pci_device_bar(identifier, PCI::HeaderType0BaseRegister::BAR0, 0x100));
    return adopt_nonnull_ref_or_enomem(new (nothrow) VIAAC97(identifier, move(io_window)));
}

VIAAC97::VIAAC97(PCI::DeviceIdentifier const& identifier, NonnullOwnPtr<IOWindow> io_window)
    : PCI::Device(identifier)
    , m_io_window(move(io_window))
{
}

VIAAC97::~VIAAC97() = default;

ErrorOr<void> VIAAC97::wait_for_codec()
{
    for (unsigned attempt = 0; attempt < 10000; ++attempt) {
        if (!(m_io_window->read32(codec_command_register) & codec_busy))
            return {};
        microseconds_delay(1);
    }
    return ETIMEDOUT;
}

ErrorOr<u16> VIAAC97::read_codec(u8 register_offset)
{
    TRY(wait_for_codec());
    m_io_window->write32(codec_command_register, codec_read_command | (static_cast<u32>(register_offset) << 16));
    for (unsigned attempt = 0; attempt < 10000; ++attempt) {
        auto result = m_io_window->read32(codec_command_register);
        if (!(result & codec_busy) && (result & codec_data_valid))
            return static_cast<u16>(result);
        microseconds_delay(1);
    }
    return ETIMEDOUT;
}

ErrorOr<void> VIAAC97::write_codec(u8 register_offset, u16 value)
{
    TRY(wait_for_codec());
    m_io_window->write32(codec_command_register, (static_cast<u32>(register_offset) << 16) | value);
    return wait_for_codec();
}

UNMAP_AFTER_INIT ErrorOr<void> VIAAC97::initialize(Badge<AudioManagement>)
{
    PCI::enable_io_space(device_identifier());
    PCI::enable_bus_mastering(device_identifier());

    auto read_pci_config = [&](u8 offset) {
        SpinlockLocker locker(device_identifier().operation_lock());
        return PCI::Access::the().read8_field(device_identifier(), offset);
    };
    auto write_pci_config = [&](u8 offset, u8 value) {
        SpinlockLocker locker(device_identifier().operation_lock());
        PCI::Access::the().write8_field(device_identifier(), offset, value);
    };

    // An unready codec needs the VIA AC-link reset sequence before playback.
    if (!(read_pci_config(pci_ac_link_status_register) & 1)) {
        write_pci_config(pci_ac_link_control_register, 0xe0);
        microseconds_delay(100);
        write_pci_config(pci_ac_link_control_register, 0);
        microseconds_delay(100);
    }
    write_pci_config(pci_ac_link_control_register, 0xcc);
    bool codec_ready = false;
    for (unsigned attempt = 0; attempt < 7500; ++attempt) {
        if (read_pci_config(pci_ac_link_status_register) & 1) {
            codec_ready = true;
            break;
        }
        microseconds_delay(100);
    }
    if (!codec_ready)
        return ETIMEDOUT;

    TRY(write_codec(codec_reset_register, 0));
    auto extended_audio_id = TRY(read_codec(codec_extended_id_register));
    if (!(extended_audio_id & 1))
        return ENOTSUP;
    TRY(write_codec(codec_extended_control_register, TRY(read_codec(codec_extended_control_register)) | 1));
    TRY(write_codec(codec_master_volume_register, 0));
    TRY(write_codec(codec_pcm_volume_register, 0));

    m_io_window->write8(playback_control_register, playback_terminate);
    m_io_window->write8(playback_status_register, playback_end_of_list | playback_stopped | 1);
    m_io_window->write8(playback_type_register, playback_stereo_16bit);

    m_audio_channel = TRY(AudioChannel::create(*this, 0));
    dmesgln_pci(*this, "PCM output ready at {} Hz", m_sample_rate);
    return {};
}

RefPtr<AudioChannel> VIAAC97::audio_channel(u32 index) const
{
    return index == 0 ? m_audio_channel : nullptr;
}

ErrorOr<void> VIAAC97::set_pcm_output_sample_rate(size_t channel_index, u32 rate)
{
    if (channel_index != 0)
        return ENODEV;
    if (rate < 8000 || rate > 48000)
        return ENOTSUP;
    MutexLocker locker(m_write_lock);
    TRY(write_codec(codec_dac_rate_register, static_cast<u16>(rate)));
    m_sample_rate = TRY(read_codec(codec_dac_rate_register));
    return {};
}

ErrorOr<u32> VIAAC97::get_pcm_output_sample_rate(size_t channel_index)
{
    if (channel_index != 0)
        return Error::from_errno(ENODEV);
    MutexLocker locker(m_write_lock);
    return m_sample_rate;
}

ErrorOr<void> VIAAC97::prepare_dma_buffers()
{
    if (!m_output_buffer)
        m_output_buffer = TRY(MM.allocate_dma_buffer_pages(
            PAGE_SIZE, "VIA AC'97 PCM output"sv, Memory::Region::Access::ReadWrite, Memory::MemoryType::IO));
    if (!m_descriptor_buffer)
        m_descriptor_buffer = TRY(MM.allocate_dma_buffer_pages(
            PAGE_SIZE, "VIA AC'97 descriptors"sv, Memory::Region::Access::ReadWrite, Memory::MemoryType::IO));
    if (m_output_buffer->physical_page(0)->paddr().get() > AK::NumericLimits<u32>::max()
        || m_descriptor_buffer->physical_page(0)->paddr().get() > AK::NumericLimits<u32>::max())
        return EOVERFLOW;
    return {};
}

ErrorOr<size_t> VIAAC97::write(size_t channel_index, UserOrKernelBuffer const& data, size_t length)
{
    if (channel_index != 0)
        return Error::from_errno(ENODEV);
    if (length % 4 != 0)
        return EINVAL;
    if (length == 0)
        return 0;

    MutexLocker locker(m_write_lock);
    TRY(prepare_dma_buffers());
    auto data_address = static_cast<u32>(m_output_buffer->physical_page(0)->paddr().get());
    auto descriptor_address = static_cast<u32>(m_descriptor_buffer->physical_page(0)->paddr().get());
    auto* descriptor = reinterpret_cast<volatile u32*>(m_descriptor_buffer->vaddr().get());

    for (size_t offset = 0; offset < length;) {
        auto chunk_size = min(length - offset, static_cast<size_t>(PAGE_SIZE));
        TRY(data.read(m_output_buffer->vaddr().as_ptr(), offset, chunk_size));
        descriptor[0] = AK::convert_between_host_and_little_endian(data_address);
        descriptor[1] = AK::convert_between_host_and_little_endian(static_cast<u32>(chunk_size) | descriptor_end_of_list);
        store_memory_fence();

        m_io_window->write8(playback_control_register, playback_terminate);
        m_io_window->write8(playback_status_register, playback_end_of_list | playback_stopped | 1);
        m_io_window->write32(playback_table_register, descriptor_address);
        m_io_window->write8(playback_control_register, playback_start);

        bool completed = false;
        for (unsigned attempt = 0; attempt < 1000; ++attempt) {
            auto status = m_io_window->read8(playback_status_register);
            if (status & playback_end_of_list) {
                m_io_window->write8(playback_status_register, playback_end_of_list);
                completed = true;
                break;
            }
            if (status & playback_stopped)
                return EIO;
            (void)Thread::current()->sleep(Duration::from_milliseconds(1));
        }
        if (!completed)
            return ETIMEDOUT;
        offset += chunk_size;
    }
    return length;
}

}
