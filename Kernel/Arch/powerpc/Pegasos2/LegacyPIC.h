/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Optional.h>
#include <AK/Types.h>
#include <Kernel/Arch/powerpc/Pegasos2/HardwareAddresses.h>

namespace Kernel::PowerPC::Pegasos2 {

// Byte-wide VT8231 8259 registers in the MV64361 PCI I/O window. The caller
// supplies mapped addresses, so the same operations work in real and paged mode.
class LegacyPIC {
public:
    static constexpr u32 io_physical_base = PEGASOS2_PCI_IO_PHYSICAL_BASE;
    static constexpr u32 intack_physical_base = PEGASOS2_INTACK_PHYSICAL_BASE;
    static constexpr u32 intack_offset = PEGASOS2_INTACK_OFFSET;
    static constexpr u32 io_window_size = 0x10000u;

    LegacyPIC(volatile u8* io_ports, volatile u8* intack)
        : m_io_ports(io_ports)
        , m_intack(intack)
    {
    }

    u8 read_port(u16 port) const
    {
        auto value = m_io_ports[port];
        asm volatile("eieio" : : : "memory");
        return value;
    }

    void write_port(u16 port, u8 value) const
    {
        m_io_ports[port] = value;
        asm volatile("eieio" : : : "memory");
    }

    u8 master_mask() const { return read_port(master_mask_port); }
    u8 slave_mask() const { return read_port(slave_mask_port); }

    void set_masks(u8 master, u8 slave)
    {
        write_port(slave_mask_port, slave);
        write_port(master_mask_port, master);
        m_master_mask = master;
        m_slave_mask = slave;
    }

    void initialize()
    {
        set_masks(0xff, 0xff);
        write_port(master_command_port, 0x11);
        write_port(master_mask_port, 0x00);
        write_port(master_mask_port, 0x04);
        write_port(master_mask_port, 0x01);
        write_port(slave_command_port, 0x11);
        write_port(slave_mask_port, 0x08);
        write_port(slave_mask_port, 0x02);
        write_port(slave_mask_port, 0x01);
        set_masks(0xff, 0xff);
        write_port(master_command_port, 0x0b); // Read the in-service register.
        write_port(slave_command_port, 0x0b);
        m_level_triggered = read_port(0x4d0) | (static_cast<u16>(read_port(0x4d1)) << 8);
    }

    bool enable(u8 irq)
    {
        if (irq >= 16 || irq == 2)
            return false;
        if (irq < 8)
            m_master_mask &= ~(1u << irq);
        else {
            m_slave_mask &= ~(1u << (irq - 8));
            m_master_mask &= ~(1u << 2);
        }
        set_masks(m_master_mask, m_slave_mask);
        return true;
    }

    bool disable(u8 irq)
    {
        if (irq >= 16 || irq == 2)
            return false;
        if (irq < 8)
            m_master_mask |= 1u << irq;
        else {
            m_slave_mask |= 1u << (irq - 8);
            if (m_slave_mask == 0xff)
                m_master_mask |= 1u << 2;
        }
        set_masks(m_master_mask, m_slave_mask);
        return true;
    }

    Optional<u8> acknowledge()
    {
        u8 irq = *m_intack;
        asm volatile("eieio" : : : "memory");
        if (irq >= 16)
            return {};
        auto isr = read_port(irq < 8 ? master_command_port : slave_command_port);
        if (!(isr & (1u << (irq & 7)))) {
            if (irq == 15)
                write_port(master_command_port, 0x20); // Spurious slave IRQ.
            return {};
        }
        if (!is_level_triggered(irq))
            send_eoi(irq);
        return irq;
    }

    void end_of_interrupt(u8 irq)
    {
        if (irq < 16 && is_level_triggered(irq))
            send_eoi(irq);
    }

    bool is_level_triggered(u8 irq) const
    {
        return irq < 16 && (m_level_triggered & (1u << irq));
    }

private:
    void send_eoi(u8 irq)
    {
        if (irq >= 8)
            write_port(slave_command_port, 0x20);
        write_port(master_command_port, 0x20);
    }

    static constexpr u16 master_command_port = 0x20;
    static constexpr u16 master_mask_port = 0x21;
    static constexpr u16 slave_command_port = 0xa0;
    static constexpr u16 slave_mask_port = 0xa1;

    volatile u8* m_io_ports;
    volatile u8* m_intack;
    u16 m_level_triggered { 0 };
    u8 m_master_mask { 0xff };
    u8 m_slave_mask { 0xff };
};

}
