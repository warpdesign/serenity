/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/SetOnce.h>
#include <Kernel/Arch/powerpc/BootHandoff.h>
#include <Kernel/Arch/powerpc/InterruptManagement.h>
#include <Kernel/Arch/powerpc/Pegasos2/PCIHostController.h>
#include <Kernel/Arch/powerpc/Pegasos2/InterruptController.h>
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

    auto south_controller = south.release_value();
    auto agp_controller = agp.release_value();
    if (south_controller->assign_unconfigured_resources((*bridges)[0]).is_error()
        || agp_controller->assign_unconfigured_resources((*bridges)[1]).is_error()) {
        dmesgln("PCI: Failed to assign Pegasos 2 device resources");
        g_pci_access_io_probe_failed.set();
        return;
    }

    // Firmware may leave add-on PCI devices' Interrupt Line register unset.
    // Read the VT8231's actual PIRQ routing instead of assuming an IRQ number.
    auto south_bus = (*bridges)[0].start_bus;
    if (south_controller->read16_field(south_bus, 12, 0, 0) == 0x1106
        && south_controller->read16_field(south_bus, 12, 0, 2) == 0x8231) {
        auto route_a = south_controller->read8_field(south_bus, 12, 0, 0x55);
        auto route_bc = south_controller->read8_field(south_bus, 12, 0, 0x56);
        auto route_d = south_controller->read8_field(south_bus, 12, 0, 0x57);
        Array<u8, 4> pirq_lines { static_cast<u8>(route_a >> 4), static_cast<u8>(route_bc & 0xf),
            static_cast<u8>(route_bc >> 4), static_cast<u8>(route_d >> 4) };
        auto controller = InterruptManagement::the().get_responsible_irq_controller(InterruptNumber { 9 });
        if (!static_cast<PowerPC::Pegasos2::InterruptController&>(*controller).configure_pci_interrupts(pirq_lines)) {
            dmesgln("PCI: Pegasos 2 could not configure level-triggered PCI interrupts");
            g_pci_access_io_probe_failed.set();
            return;
        }
        auto assigned = south_controller->assign_unconfigured_interrupt_lines((*bridges)[0], pirq_lines)
            + agp_controller->assign_unconfigured_interrupt_lines((*bridges)[1], pirq_lines);
        dmesgln("PCI: Pegasos 2 routed {} unconfigured PCI interrupt lines", assigned);
    }

    auto& access = Access::the();
    access.add_host_controller(move(south_controller));
    access.add_host_controller(move(agp_controller));
    access.rescan_hardware();
    PCIBusSysFSDirectory::initialize();
    MUST(PCI::enumerate([](DeviceIdentifier const& identifier) {
        dmesgln("{} {}", identifier.address(), identifier.hardware_id());
    }));
}

}
