/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibPartition/MBRPartitionTable.h>

extern "C" int powerpc_mbr_wire_format_test()
{
    u8 bytes[sizeof(Partition::MBRPartitionTable::Header)] {};
    bytes[510] = 0x55;
    bytes[511] = 0xaa;
    bytes[454] = 0x78;
    bytes[455] = 0x56;
    bytes[456] = 0x34;
    bytes[457] = 0x12;
    bytes[458] = 0x00;
    bytes[459] = 0x10;
    auto const& header = *reinterpret_cast<Partition::MBRPartitionTable::Header const*>(bytes);
    return header.mbr_signature == 0xaa55
        && header.entry[0].offset == 0x12345678u
        && header.entry[0].length == 0x1000u;
}
