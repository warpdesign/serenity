/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/ByteBuffer.h>
#include <AK/Debug.h>
#include <LibPartition/RDBPartitionTable.h>

namespace Partition {

// Bounds the partition list walk, so a looping or corrupt list cannot hang the caller.
static constexpr size_t max_partitions = 128;

ErrorOr<NonnullOwnPtr<RDBPartitionTable>> RDBPartitionTable::try_to_initialize(PartitionableDevice device)
{
    auto table = TRY(adopt_nonnull_own_or_enomem(new (nothrow) RDBPartitionTable(move(device))));
    if (!table->is_valid())
        return Error::from_errno(EINVAL);
    return table;
}

RDBPartitionTable::RDBPartitionTable(PartitionableDevice device)
    : PartitionTable(move(device))
{
    m_valid = !initialize().is_error();
}

// Each RDB block is valid when its first summed_longs big-endian longwords add up to zero.
static bool is_checksum_valid(ReadonlyBytes block)
{
    auto const& header = *reinterpret_cast<RDBPartitionTable::BlockHeader const*>(block.data());
    u32 summed_longs = header.summed_longs;
    if (summed_longs < sizeof(RDBPartitionTable::BlockHeader) / sizeof(u32) || summed_longs > block.size() / sizeof(u32))
        return false;
    u32 sum = 0;
    auto const* longs = reinterpret_cast<BigEndian<u32> const*>(block.data());
    for (u32 i = 0; i < summed_longs; ++i)
        sum += longs[i];
    return sum == 0;
}

ErrorOr<void> RDBPartitionTable::initialize()
{
    size_t device_block_size = block_size();
    if (device_block_size < sizeof(PartitionBlock))
        return Error::from_errno(ENOTSUP);
    auto buffer = TRY(ByteBuffer::create_zeroed(device_block_size));

    bool found = false;
    for (u32 block = 0; block < search_limit; ++block) {
        TRY(m_device.read_block(block, buffer.bytes()));
        auto const& rigid_disk = *reinterpret_cast<RigidDiskBlock const*>(buffer.data());
        if (rigid_disk.header.id == rigid_disk_block_id && is_checksum_valid(buffer.bytes())) {
            found = true;
            break;
        }
    }
    if (!found)
        return Error::from_errno(ENOENT);

    auto const& rigid_disk = *reinterpret_cast<RigidDiskBlock const*>(buffer.data());
    if (rigid_disk.block_bytes != device_block_size) {
        dbgln("RDBPartitionTable: RDB block size {} does not match the device block size {}", static_cast<u32>(rigid_disk.block_bytes), device_block_size);
        return Error::from_errno(ENOTSUP);
    }

    u32 next = rigid_disk.partition_list;
    while (next != end_of_list) {
        if (m_partitions.size() >= max_partitions)
            return Error::from_errno(ELOOP);
        TRY(m_device.read_block(next, buffer.bytes()));
        auto const& partition = *reinterpret_cast<PartitionBlock const*>(buffer.data());
        if (partition.header.id != partition_block_id || !is_checksum_valid(buffer.bytes()))
            return Error::from_errno(EINVAL);
        next = partition.next;

        auto const& environment = partition.environment;
        // The environment must be long enough to hold the DosType (index 16).
        if (environment.table_size < 16)
            continue;
        u64 environment_block_bytes = static_cast<u64>(environment.size_block) * sizeof(u32);
        u64 blocks_per_cylinder = static_cast<u64>(environment.surfaces) * environment.blocks_per_track;
        u32 low_cylinder = environment.low_cylinder;
        u32 high_cylinder = environment.high_cylinder;
        if (environment_block_bytes == 0 || blocks_per_cylinder == 0 || high_cylinder < low_cylinder)
            continue;
        u64 start_byte = low_cylinder * blocks_per_cylinder * environment_block_bytes;
        u64 end_byte = (static_cast<u64>(high_cylinder) + 1) * blocks_per_cylinder * environment_block_bytes;
        if (start_byte % device_block_size != 0 || end_byte % device_block_size != 0)
            continue;

        Array<u8, 16> type {};
        u32 dos_type = environment.dos_type;
        type[0] = dos_type >> 24;
        type[1] = dos_type >> 16;
        type[2] = dos_type >> 8;
        type[3] = dos_type;
        u64 start_block = start_byte / device_block_size;
        u64 end_block = end_byte / device_block_size - 1;
        dbgln("Detected RDB partition, DosType {:08x}, blocks {}-{}", dos_type, start_block, end_block);
        TRY(m_partitions.try_empend(start_block, end_block, type));
    }
    return {};
}

}
