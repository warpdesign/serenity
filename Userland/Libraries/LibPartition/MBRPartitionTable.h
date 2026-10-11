/*
 * Copyright (c) 2020-2026, Liav A. <liavalb@hotmail.co.il>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/Endian.h>
#include <AK/StdLibExtraDetails.h>
#include <LibPartition/PartitionTable.h>

namespace Partition {

class MBRPartitionTable : public PartitionTable {
public:
    struct Entry {
        u8 status;
        u8 chs1[3];
        u8 type;
        u8 chs2[3];
        LittleEndian<u32> offset;
        LittleEndian<u32> length;
    };
    static_assert(AssertSize<Entry, 16>());
    struct Header {
        u8 code1[218];
        LittleEndian<u16> ts_zero;
        u8 ts_drive;
        u8 ts_seconds;
        u8 ts_minutes;
        u8 ts_hours;
        u8 code2[216];
        LittleEndian<u32> disk_signature;
        LittleEndian<u16> disk_signature_zero;
        Entry entry[4];
        LittleEndian<u16> mbr_signature;
    };
    static_assert(AssertSize<Header, 512>());

public:
    ~MBRPartitionTable();

    static ErrorOr<NonnullOwnPtr<MBRPartitionTable>> try_to_initialize(PartitionableDevice);
    static OwnPtr<MBRPartitionTable> try_to_initialize(PartitionableDevice, u32 start_lba);
    explicit MBRPartitionTable(PartitionableDevice);
    MBRPartitionTable(PartitionableDevice, u32 start_lba);

    bool is_protective_mbr() const;
    bool contains_ebr() const;
    virtual bool is_valid() const override { return m_valid; }

protected:
    Header const& header() const;
    bool is_header_valid() const { return m_header_valid; }

private:
    bool read_boot_record();
    bool initialize();
    bool m_valid { false };
    bool m_header_valid { false };
    u32 const m_start_lba;
    Header m_cached_header;
};

}
