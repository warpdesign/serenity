/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/OwnPtr.h>
#include <Kernel/Bus/PCI/Device.h>
#include <Kernel/Devices/Audio/Controller.h>
#include <Kernel/Locking/Mutex.h>

namespace Kernel {
class IOWindow;
namespace Memory {
class Region;
}
}

namespace Kernel::PowerPC::Pegasos2 {

class VIAAC97 final : public AudioController, public PCI::Device {
public:
    static ErrorOr<bool> probe(PCI::DeviceIdentifier const&);
    static ErrorOr<NonnullRefPtr<AudioController>> create(PCI::DeviceIdentifier const&);

    virtual ~VIAAC97() override;
    virtual StringView device_name() const override { return "VIA AC'97"sv; }

private:
    VIAAC97(PCI::DeviceIdentifier const&, NonnullOwnPtr<IOWindow>);

    virtual ErrorOr<void> initialize(Badge<AudioManagement>) override;
    virtual RefPtr<AudioChannel> audio_channel(u32 index) const override;
    virtual ErrorOr<size_t> write(size_t channel_index, UserOrKernelBuffer const&, size_t length) override;
    virtual ErrorOr<void> set_pcm_output_sample_rate(size_t channel_index, u32 rate) override;
    virtual ErrorOr<u32> get_pcm_output_sample_rate(size_t channel_index) override;

    ErrorOr<void> wait_for_codec();
    ErrorOr<u16> read_codec(u8 register_offset);
    ErrorOr<void> write_codec(u8 register_offset, u16 value);
    ErrorOr<void> prepare_dma_buffers();

    NonnullOwnPtr<IOWindow> m_io_window;
    RefPtr<AudioChannel> m_audio_channel;
    OwnPtr<Memory::Region> m_output_buffer;
    OwnPtr<Memory::Region> m_descriptor_buffer;
    Mutex m_write_lock { "VIA AC'97"sv };
    u32 m_sample_rate { 48000 };
};

}
