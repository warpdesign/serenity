/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <Kernel/Arch/powerpc/Pegasos2/OpenFirmwarePCIRanges.h>
#include <Kernel/Bus/PCI/Controller/HostController.h>
#include <Kernel/Memory/TypedMapping.h>

namespace Kernel::PowerPC::Pegasos2 {

class PCIHostController final : public PCI::HostController {
public:
    static ErrorOr<NonnullOwnPtr<PCIHostController>> create(PhysicalAddress registers_base, PowerPCPCIHostBridgeInfo const&);

private:
    PCIHostController(PCI::Domain const&, Memory::TypedMapping<volatile u32[]>, u16 address_offset);

    virtual void write8_field_locked(PCI::BusNumber, PCI::DeviceNumber, PCI::FunctionNumber, u32 field, u8 value) override;
    virtual void write16_field_locked(PCI::BusNumber, PCI::DeviceNumber, PCI::FunctionNumber, u32 field, u16 value) override;
    virtual void write32_field_locked(PCI::BusNumber, PCI::DeviceNumber, PCI::FunctionNumber, u32 field, u32 value) override;
    virtual u8 read8_field_locked(PCI::BusNumber, PCI::DeviceNumber, PCI::FunctionNumber, u32 field) override;
    virtual u16 read16_field_locked(PCI::BusNumber, PCI::DeviceNumber, PCI::FunctionNumber, u32 field) override;
    virtual u32 read32_field_locked(PCI::BusNumber, PCI::DeviceNumber, PCI::FunctionNumber, u32 field) override;

    volatile u32* address_register() { return m_registers.ptr() + m_address_offset / sizeof(u32); }
    volatile u32* data_register() { return address_register() + 1; }

    Memory::TypedMapping<volatile u32[]> m_registers;
    u16 m_address_offset;
};

}
