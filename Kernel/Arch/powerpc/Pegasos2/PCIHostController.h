/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Array.h>
#include <Kernel/Arch/powerpc/Pegasos2/OpenFirmwarePCIRanges.h>
#include <Kernel/Arch/powerpc/Pegasos2/RTAS.h>
#include <Kernel/Bus/PCI/Controller/HostController.h>
#include <Kernel/Memory/TypedMapping.h>

namespace Kernel::PowerPC::Pegasos2 {

class PCIHostController final : public PCI::HostController {
public:
    static ErrorOr<NonnullOwnPtr<PCIHostController>> create(PhysicalAddress registers_base, PowerPCPCIHostBridgeInfo const&, Optional<PowerPCRtasInfo> const&);
    ErrorOr<void> assign_unconfigured_resources(PowerPCPCIHostBridgeInfo const&);
    size_t assign_unconfigured_interrupt_lines(PowerPCPCIHostBridgeInfo const&, Array<u8, 4> const& pirq_lines);
    u32 last_rtas_status() const { return m_last_rtas_status; }
    u32 last_rtas_value() const { return m_last_rtas_value; }

private:
    PCIHostController(PCI::Domain const&, Memory::TypedMapping<volatile u32[]>, u16 address_offset,
        Optional<PowerPCRtasInfo> const&, Memory::TypedMapping<volatile PowerPCRtasArguments>);
    Optional<u32> read_rtas_config(PCI::BusNumber, PCI::DeviceNumber, PCI::FunctionNumber, u32 field, u32 size);
    bool write_rtas_config(PCI::BusNumber, PCI::DeviceNumber, PCI::FunctionNumber, u32 field, u32 size, u32 value);

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
    Optional<PowerPCRtasInfo> m_rtas;
    Memory::TypedMapping<volatile PowerPCRtasArguments> m_rtas_arguments;
    u32 m_last_rtas_status { 0xffffffffu };
    u32 m_last_rtas_value { 0xffffffffu };
};

}
