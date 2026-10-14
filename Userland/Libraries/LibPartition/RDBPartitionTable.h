/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Endian.h>
#include <AK/Error.h>
#include <AK/StdLibExtraDetails.h>
#include <LibPartition/PartitionTable.h>

namespace Partition {

// Amiga Rigid Disk Block, as used by AmigaOS, MorphOS and the Pegasos SmartFirmware.
// Each partition's 4-byte DosType (for example "DOS\3" or "LNX\0") is stored in the first bytes
// of its 16-byte partition type.
class RDBPartitionTable final : public PartitionTable {
public:
    static constexpr u32 rigid_disk_block_id = 0x5244534b; // "RDSK"
    static constexpr u32 partition_block_id = 0x50415254;  // "PART"
    static constexpr u32 end_of_list = 0xffffffff;
    // The RDSK block may be in any of the first 16 blocks of the disk.
    static constexpr u32 search_limit = 16;

    struct [[gnu::packed]] BlockHeader {
        BigEndian<u32> id;
        BigEndian<u32> summed_longs;
        BigEndian<i32> checksum;
        BigEndian<u32> host_id;
    };
    static_assert(AssertSize<BlockHeader, 16>());

    struct [[gnu::packed]] RigidDiskBlock {
        BlockHeader header;
        BigEndian<u32> block_bytes;
        BigEndian<u32> flags;
        BigEndian<u32> bad_block_list;
        BigEndian<u32> partition_list;
        BigEndian<u32> file_system_header_list;
        BigEndian<u32> drive_init;
        BigEndian<u32> boot_block_list;
    };

    struct [[gnu::packed]] DosEnvironment {
        BigEndian<u32> table_size;
        BigEndian<u32> size_block; // In longwords.
        BigEndian<u32> sector_origin;
        BigEndian<u32> surfaces;
        BigEndian<u32> sectors_per_block;
        BigEndian<u32> blocks_per_track;
        BigEndian<u32> reserved;
        BigEndian<u32> pre_alloc;
        BigEndian<u32> interleave;
        BigEndian<u32> low_cylinder;
        BigEndian<u32> high_cylinder;
        BigEndian<u32> buffers;
        BigEndian<u32> buffer_memory_type;
        BigEndian<u32> max_transfer;
        BigEndian<u32> mask;
        BigEndian<i32> boot_priority;
        BigEndian<u32> dos_type;
    };
    static_assert(AssertSize<DosEnvironment, 68>());

    struct [[gnu::packed]] PartitionBlock {
        BlockHeader header;
        BigEndian<u32> next;
        BigEndian<u32> flags;
        BigEndian<u32> reserved1[2];
        BigEndian<u32> device_flags;
        u8 drive_name[32]; // BCPL string: a length byte followed by the name.
        BigEndian<u32> reserved2[15];
        DosEnvironment environment;
    };
    static_assert(__builtin_offsetof(PartitionBlock, environment) == 128);

    static ErrorOr<NonnullOwnPtr<RDBPartitionTable>> try_to_initialize(PartitionableDevice);
    explicit RDBPartitionTable(PartitionableDevice);

    virtual bool is_valid() const override { return m_valid; }

private:
    ErrorOr<void> initialize();

    bool m_valid { false };
};

}
