/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Endian.h>
#include <Kernel/FileSystem/Ext2FS/Definitions.h>

extern "C" int powerpc_ext2_wire_format_test()
{
    static_assert(sizeof(ext2_super_block) == 1024);
    static_assert(sizeof(ext2_group_desc) == 32);
    static_assert(sizeof(ext2_inode) == 128);

    u8 superblock_bytes[sizeof(ext2_super_block)] {};
    superblock_bytes[56] = 0x53;
    superblock_bytes[57] = 0xef;
    superblock_bytes[4] = 0x78;
    superblock_bytes[5] = 0x56;
    superblock_bytes[6] = 0x34;
    superblock_bytes[7] = 0x12;
    auto& superblock = *reinterpret_cast<ext2_super_block*>(superblock_bytes);
    if (superblock.s_magic != EXT2_SUPER_MAGIC || superblock.s_blocks_count != 0x12345678)
        return 0;
    superblock.s_state = 0x0102;
    if (superblock_bytes[58] != 0x02 || superblock_bytes[59] != 0x01)
        return 0;
    superblock.s_max_mnt_count = -2;
    if (superblock_bytes[54] != 0xfe || superblock_bytes[55] != 0xff)
        return 0;
    superblock.s_mmp_block = 0x0102030405060708ull;
    auto mmp_bytes = superblock.s_mmp_block.bytes();
    if (mmp_bytes[0] != 0x08 || mmp_bytes[7] != 0x01)
        return 0;

    u8 descriptor_bytes[sizeof(ext2_group_desc)] {};
    descriptor_bytes[8] = 0x04;
    descriptor_bytes[9] = 0x03;
    descriptor_bytes[10] = 0x02;
    descriptor_bytes[11] = 0x01;
    auto& descriptor = *reinterpret_cast<ext2_group_desc*>(descriptor_bytes);
    if (descriptor.bg_inode_table != 0x01020304)
        return 0;

    u8 inode_bytes[sizeof(ext2_inode)] {};
    inode_bytes[40] = 0x44;
    inode_bytes[41] = 0x33;
    inode_bytes[42] = 0x22;
    inode_bytes[43] = 0x11;
    auto& inode = *reinterpret_cast<ext2_inode*>(inode_bytes);
    if (inode.i_block[0] != 0x11223344)
        return 0;

    u8 directory_bytes[8] { 0x09, 0x00, 0x00, 0x00, 0x08, 0x00, 0x01, 0x01 };
    auto& directory_entry = *reinterpret_cast<ext2_dir_entry_2*>(directory_bytes);
    if (directory_entry.inode != 9 || directory_entry.rec_len != 8 || directory_entry.name_len != 1)
        return 0;
    directory_entry.rec_len = 0x1234;
    if (directory_bytes[4] != 0x34 || directory_bytes[5] != 0x12)
        return 0;

    u8 pointer_bytes[4] { 0x78, 0x56, 0x34, 0x12 };
    auto& indirect_pointer = *reinterpret_cast<__u32*>(pointer_bytes);
    if (indirect_pointer != 0x12345678)
        return 0;
    indirect_pointer = 0x01020304;
    return pointer_bytes[0] == 0x04 && pointer_bytes[1] == 0x03 && pointer_bytes[2] == 0x02 && pointer_bytes[3] == 0x01;
}
