/*
 * Copyright (c) 2022-2026, Liav A. <liavalb@hotmail.co.il>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/IntrusiveList.h>
#include <AK/NonnullRefPtr.h>
#include <AK/Vector.h>
#include <Kernel/Bus/PCI/Definitions.h>
#include <Kernel/Devices/Audio/Controller.h>

namespace Kernel {

struct PCIAudioDriverInitializer {
    ErrorOr<bool> (*probe)(PCI::DeviceIdentifier const&);
    ErrorOr<NonnullRefPtr<AudioController>> (*create)(PCI::DeviceIdentifier const&);
};

class AudioManagement {

public:
    AudioManagement();
    static AudioManagement& the();

    static MinorNumber generate_storage_minor_number();

    ErrorOr<void> register_pci_driver(PCIAudioDriverInitializer);
    bool initialize();

private:
    ErrorOr<NonnullRefPtr<AudioController>> determine_audio_device(PCI::DeviceIdentifier const& device_identifier) const;

    void enumerate_hardware_controllers();
    Vector<PCIAudioDriverInitializer, 1> m_platform_initializers;
    RecursiveSpinlockProtected<IntrusiveList<&AudioController::m_node>, LockRank::None> m_controllers_list;
};

}
