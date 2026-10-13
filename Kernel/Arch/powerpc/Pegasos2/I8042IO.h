/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/Types.h>
#include <Kernel/Arch/powerpc/Pegasos2/HardwareAddresses.h>
#include <Kernel/Memory/TypedMapping.h>

namespace Kernel {

// The Pegasos 2's VT8231 south bridge decodes the legacy keyboard controller ports in the PCI I/O window.
class I8042IO {
public:
    static ErrorOr<I8042IO> create()
    {
        return I8042IO { TRY(Memory::map_typed<volatile u8[]>(PhysicalAddress { PEGASOS2_PCI_IO_PHYSICAL_BASE }, 0x1000, Memory::Region::Access::ReadWrite)) };
    }

    u8 read(u8 port)
    {
        u8 value = m_io_ports.ptr()[port];
        asm volatile("eieio" : : : "memory");
        return value;
    }

    void write(u8 port, u8 data)
    {
        m_io_ports.ptr()[port] = data;
        asm volatile("eieio" : : : "memory");
    }

private:
    explicit I8042IO(Memory::TypedMapping<volatile u8[]> io_ports)
        : m_io_ports(move(io_ports))
    {
    }

    Memory::TypedMapping<volatile u8[]> m_io_ports;
};

}
