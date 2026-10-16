/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <Kernel/Devices/Storage/IDE/IDEChannel.h>
#include <Kernel/Devices/Storage/StorageDevice.h>
#include <Kernel/Memory/Region.h>

namespace Kernel {

class IDEDevice final : public StorageDevice {
    friend class Device;

public:
    static ErrorOr<NonnullRefPtr<IDEDevice>> create(NonnullRefPtr<IDEChannel>, IDEChannel::Drive, IDEChannel::Identity, LUNAddress, u32 hardware_relative_controller_id);

    // ^StorageDevice
    virtual CommandSet command_set() const override { return CommandSet::ATA; }

    // ^BlockDevice
    virtual void start_request(AsyncBlockDeviceRequest&) override;

private:
    IDEDevice(NonnullRefPtr<IDEChannel>, IDEChannel::Drive, IDEChannel::Identity, NonnullOwnPtr<Memory::Region> bounce_buffer, PhysicalAddress bounce_buffer_physical, LUNAddress, u32 hardware_relative_controller_id);

    // ^DiskDevice
    virtual StringView class_name() const override { return "IDEDevice"sv; }
    virtual size_t max_transfer_size() const override { return IDEChannel::max_dma_transfer_size; }

    ErrorOr<void> handle_request(AsyncBlockDeviceRequest&);

    NonnullRefPtr<IDEChannel> const m_channel;
    IDEChannel::Drive const m_drive;
    bool const m_use_lba48;
    // Physically contiguous and cacheable; the channel keeps its caches coherent around DMA.
    NonnullOwnPtr<Memory::Region> m_bounce_buffer;
    PhysicalAddress const m_bounce_buffer_physical;
};

}
