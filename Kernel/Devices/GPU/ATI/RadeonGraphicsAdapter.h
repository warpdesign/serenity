/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <Kernel/Bus/PCI/Device.h>
#include <Kernel/Devices/GPU/ATI/RadeonDefinitions.h>
#include <Kernel/Devices/GPU/GPUDevice.h>

namespace Kernel {

class RadeonGraphicsAdapter final : public GPUDevice
    , public PCI::Device {
public:
    static ErrorOr<bool> probe(PCI::DeviceIdentifier const&);
    static ErrorOr<NonnullLockRefPtr<GPUDevice>> create(PCI::DeviceIdentifier const&);
    virtual ~RadeonGraphicsAdapter() = default;
    virtual StringView device_name() const override { return "RadeonGraphicsAdapter"sv; }

private:
    explicit RadeonGraphicsAdapter(PCI::DeviceIdentifier const&);

    ErrorOr<void> initialize_adapter(PCI::DeviceIdentifier const&, Radeon::Family);

    RefPtr<DisplayConnector> m_display_connector;
};

}
