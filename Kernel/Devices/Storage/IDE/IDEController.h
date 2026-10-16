/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Vector.h>
#include <Kernel/Bus/PCI/Definitions.h>
#include <Kernel/Devices/Storage/IDE/IDEChannel.h>
#include <Kernel/Devices/Storage/StorageController.h>

namespace Kernel {

class IDEDevice;

// A PCI IDE controller whose channels run in PCI native mode, such as the Pegasos 2's VT8231.
class IDEController final : public StorageController {
public:
    using IRQOverride = Optional<InterruptNumber> (*)(PCI::DeviceIdentifier const&, u8 channel_index);

    static void set_irq_override(IRQOverride);
    static bool is_handled(PCI::DeviceIdentifier const&);
    static ErrorOr<NonnullRefPtr<IDEController>> initialize(PCI::DeviceIdentifier const&);

    // ^StorageController
    virtual LockRefPtr<StorageDevice> device(u32 index) const override;
    virtual size_t devices_count() const override { return m_devices.size(); }

protected:
    virtual void complete_current_request(AsyncDeviceRequest::RequestResult) override { VERIFY_NOT_REACHED(); }

private:
    IDEController();

    ErrorOr<void> initialize_channel(PCI::DeviceIdentifier const&, u8 channel_index);
    void configure_dma(PCI::DeviceIdentifier const&, IDEChannel&, u8 channel_index, IDEChannel::Drive, IDEChannel::Identity const&);

    Vector<NonnullRefPtr<IDEDevice>> m_devices;
};

}
