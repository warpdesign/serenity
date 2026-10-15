/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Devices/Device.h>
#include <Kernel/Devices/Storage/IDE/IDEDevice.h>
#include <Kernel/Memory/MemoryManager.h>
#include <Kernel/Tasks/WorkQueue.h>

namespace Kernel {

static constexpr size_t bounce_buffer_size = IDEChannel::max_dma_transfer_size;

ErrorOr<NonnullRefPtr<IDEDevice>> IDEDevice::create(NonnullRefPtr<IDEChannel> channel, IDEChannel::Drive drive, IDEChannel::Identity identity, LUNAddress lun, u32 hardware_relative_controller_id)
{
    Vector<NonnullRefPtr<Memory::PhysicalRAMPage>> pages;
    auto bounce_buffer = TRY(MM.allocate_dma_buffer_pages(bounce_buffer_size, "IDE bounce buffer"sv, Memory::Region::Access::ReadWrite, pages, Memory::MemoryType::Normal));
    auto bounce_buffer_physical = pages.first()->paddr();
    return Device::try_create_device<IDEDevice>(move(channel), drive, identity, move(bounce_buffer), bounce_buffer_physical, lun, hardware_relative_controller_id);
}

IDEDevice::IDEDevice(NonnullRefPtr<IDEChannel> channel, IDEChannel::Drive drive, IDEChannel::Identity identity, NonnullOwnPtr<Memory::Region> bounce_buffer, PhysicalAddress bounce_buffer_physical, LUNAddress lun, u32 hardware_relative_controller_id)
    : StorageDevice(lun, hardware_relative_controller_id, IDEChannel::sector_size, identity.sector_count - 1)
    , m_channel(move(channel))
    , m_drive(drive)
    , m_use_lba48(identity.supports_lba48)
    , m_bounce_buffer(move(bounce_buffer))
    , m_bounce_buffer_physical(bounce_buffer_physical)
{
}

void IDEDevice::start_request(AsyncBlockDeviceRequest& request)
{
    // Transfers poll the drive, so run them on the I/O work queue instead of the caller's context.
    auto result = g_io_work->try_queue([this, request = NonnullRefPtr(request)]() {
        auto result = handle_request(*request);
        if (result.is_error())
            dmesgln("IDEDevice: Request for block {} failed: {}", request->block_index(), result.error());
        request->complete(result.is_error() ? AsyncDeviceRequest::Failure : AsyncDeviceRequest::Success);
    });
    if (result.is_error())
        request.complete(AsyncDeviceRequest::Failure);
}

ErrorOr<void> IDEDevice::handle_request(AsyncBlockDeviceRequest& request)
{
    if (request.request_type() != AsyncBlockDeviceRequest::Read && request.request_type() != AsyncBlockDeviceRequest::Write)
        return Error::from_errno(EINVAL);
    if (request.buffer_size() < static_cast<size_t>(request.block_count()) * block_size())
        return Error::from_errno(EINVAL);

    auto sectors_per_chunk = bounce_buffer_size / block_size();
    for (size_t done = 0; done < request.block_count();) {
        auto count = min<size_t>(request.block_count() - done, sectors_per_chunk);
        auto bytes = Bytes { m_bounce_buffer->vaddr().as_ptr(), count * block_size() };
        auto offset = done * block_size();
        if (request.request_type() == AsyncBlockDeviceRequest::Read) {
            TRY(m_channel->read_sectors(m_drive, m_use_lba48, request.block_index() + done, count, bytes, m_bounce_buffer_physical));
            TRY(request.write_to_buffer(request.buffer(), bytes.data(), offset, bytes.size()));
        } else {
            TRY(request.read_from_buffer(request.buffer(), bytes.data(), offset, bytes.size()));
            TRY(m_channel->write_sectors(m_drive, m_use_lba48, request.block_index() + done, count, bytes, m_bounce_buffer_physical));
        }
        done += count;
    }
    return {};
}

}
