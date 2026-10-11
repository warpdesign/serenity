/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/SetOnce.h>
#include <Kernel/Arch/powerpc/BootHandoff.h>
#include <Kernel/Arch/powerpc/Pegasos2/PCIHostController.h>
#include <Kernel/Boot/BootInfo.h>
#include <Kernel/Boot/CommandLine.h>
#include <Kernel/Bus/PCI/API.h>
#include <Kernel/Bus/PCI/Access.h>
#include <Kernel/Bus/PCI/Initializer.h>
#include <Kernel/FileSystem/SysFS/Subsystems/Bus/PCI/BusDirectory.h>
#include <Kernel/Sections.h>

namespace Kernel::PCI {

READONLY_AFTER_INIT SetOnce g_pci_access_io_probe_failed;
READONLY_AFTER_INIT SetOnce g_pci_access_is_disabled_from_commandline;

UNMAP_AFTER_INIT void initialize()
{
    if (kernel_command_line().is_pci_disabled()) {
        g_pci_access_is_disabled_from_commandline.set();
        return;
    }

    auto registers_base = PowerPC::boot_mv64361_registers_base(g_boot_info);
    auto bridges = PowerPC::boot_pci_host_bridges(g_boot_info);
    if (!registers_base.has_value() || !bridges.has_value()) {
        dmesgln("PCI: Pegasos 2 firmware bridge information unavailable");
        g_pci_access_io_probe_failed.set();
        return;
    }

    auto south = PowerPC::Pegasos2::PCIHostController::create(PhysicalAddress { registers_base.value() }, (*bridges)[0]);
    auto agp = PowerPC::Pegasos2::PCIHostController::create(PhysicalAddress { registers_base.value() }, (*bridges)[1]);
    if (south.is_error() || agp.is_error()) {
        dmesgln("PCI: Failed to map Pegasos 2 host controllers");
        g_pci_access_io_probe_failed.set();
        return;
    }

    auto& access = Access::the();
    access.add_host_controller(south.release_value());
    access.add_host_controller(agp.release_value());
    access.rescan_hardware();
    PCIBusSysFSDirectory::initialize();
    MUST(PCI::enumerate([](DeviceIdentifier const& identifier) {
        dmesgln("{} {}", identifier.address(), identifier.hardware_id());
    }));
}

}
