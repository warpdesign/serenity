/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Optional.h>
#include <Kernel/Arch/powerpc/Pegasos2/HardwareAddresses.h>
#include <Kernel/Arch/powerpc/Pegasos2/RTC.h>
#include <Kernel/Debug.h>
#include <Kernel/Memory/TypedMapping.h>

namespace Kernel::PowerPC::Pegasos2::RTC {

static UnixDateTime s_boot_time = UnixDateTime::epoch();

struct Fields {
    u8 status_b;
    u8 second;
    u8 minute;
    u8 hour;
    u8 day;
    u8 month;
    u8 year;
    u8 century;

    bool operator==(Fields const&) const = default;
};

static u8 read_register(volatile u8* io_ports, u8 index)
{
    io_ports[0x70] = index;
    asm volatile("eieio" : : : "memory");
    auto value = io_ports[0x71];
    asm volatile("eieio" : : : "memory");
    return value;
}

static Optional<Fields> read_stable_fields(volatile u8* io_ports)
{
    Optional<Fields> previous;
    for (unsigned attempt = 0; attempt < 5; ++attempt) {
        bool ready = false;
        for (unsigned poll = 0; poll < 1000; ++poll) {
            if (!(read_register(io_ports, 0x0a) & 0x80)) {
                ready = true;
                break;
            }
        }
        if (!ready)
            continue;

        Fields fields {
            read_register(io_ports, 0x0b),
            read_register(io_ports, 0x00),
            read_register(io_ports, 0x02),
            read_register(io_ports, 0x04),
            read_register(io_ports, 0x07),
            read_register(io_ports, 0x08),
            read_register(io_ports, 0x09),
            read_register(io_ports, 0x32),
        };
        if (read_register(io_ports, 0x0a) & 0x80)
            continue;
        if (previous.has_value() && fields == previous.value())
            return fields;
        previous = fields;
    }
    return {};
}

static u8 bcd_to_binary(u8 value)
{
    return (value & 0x0f) + (value >> 4) * 10;
}

void initialize()
{
    auto mapping_or_error = Memory::map_typed<volatile u8[]>(
        PhysicalAddress { PEGASOS2_PCI_IO_PHYSICAL_BASE }, 0x1000, Memory::Region::Access::ReadWrite);
    if (mapping_or_error.is_error()) {
        dmesgln("Pegasos 2 RTC: could not map ISA I/O ports: {}", mapping_or_error.error());
        return;
    }
    auto mapping = mapping_or_error.release_value();
    auto fields = read_stable_fields(mapping.ptr());
    if (!fields.has_value()) {
        dmesgln("Pegasos 2 RTC: could not read a stable clock value");
        return;
    }

    auto value = fields.release_value();
    bool is_pm = value.hour & 0x80;
    value.hour &= 0x7f;
    if (!(value.status_b & 0x04)) {
        value.second = bcd_to_binary(value.second);
        value.minute = bcd_to_binary(value.minute);
        value.hour = bcd_to_binary(value.hour);
        value.day = bcd_to_binary(value.day);
        value.month = bcd_to_binary(value.month);
        value.year = bcd_to_binary(value.year);
        value.century = bcd_to_binary(value.century);
    }
    if (!(value.status_b & 0x02)) {
        value.hour %= 12;
        if (is_pm)
            value.hour += 12;
    }

    unsigned year = (value.century >= 19 && value.century <= 21 ? value.century * 100 : 2000) + value.year;
    if (value.month < 1 || value.month > 12 || value.day < 1 || value.day > 31
        || value.hour > 23 || value.minute > 59 || value.second > 59) {
        dmesgln("Pegasos 2 RTC: invalid date from CMOS");
        return;
    }

    i64 seconds = ((days_since_epoch(year, value.month, value.day) * 24 + value.hour) * 60 + value.minute) * 60 + value.second;
    s_boot_time = UnixDateTime::from_seconds_since_epoch(seconds);
    dmesgln("Pegasos 2 RTC: {:04}-{:02}-{:02} {:02}:{:02}:{:02} UTC", year, value.month, value.day, value.hour, value.minute, value.second);
}

UnixDateTime boot_time()
{
    return s_boot_time;
}

}
