#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

"""Build an Amiga RDB disk image that the Pegasos 2 SmartFirmware can boot SerenityOS from.

Layout (512-byte sectors):
  sector 0          RDSK block
  sectors 1-2       PART blocks
  partition 0       "SERBOOT", Amiga FFS (DOS\\3), bootable, holding serenity.elf
                    SmartFirmware: boot ide:0 serenity.elf
  partition 1       "SERROOT", the ext2 root filesystem (DosType LNX\\0, not mounted by AmigaOS)
                    Serenity: root=lunX:Y:Z;part1

    make-pegasos-card.py card --loader serenity.elf --root root.ext2 --output card.img
        writes a complete image; copy it to the SD card with dd.
    make-pegasos-card.py boot-only --loader serenity.elf --target card.img-or-device
        rewrites only the boot partition of an existing card made by this tool, for quick loader
        iterations. The RDB on the target must match this tool's layout.
"""

import argparse
import struct
import time
from pathlib import Path

SECTOR = 512
FIRST_PARTITION_SECTOR = 2048  # 1 MiB, leaving room for the RDB and alignment.
BOOT_PARTITION_SECTORS = 65536  # 32 MiB, within one FFS root block's 25 bitmap pages.
LONGS = SECTOR // 4
HASH_TABLE_SIZE = LONGS - 56
DOS3 = 0x444f5303  # FFS, international mode
LNX0 = 0x4c4e5800  # Linux native, used for the ext2 root
T_HEADER, T_LIST = 2, 16
ST_ROOT, ST_FILE = 1, -3 & 0xffffffff


def put32(block, index, value):
    struct.pack_into(">I", block, index * 4, value & 0xffffffff)


def set_checksum(block, index):
    put32(block, index, 0)
    total = sum(struct.unpack(f">{LONGS}I", block)) & 0xffffffff
    put32(block, index, -total)


def bcpl(block, offset, text, limit):
    data = text.encode("latin1")[:limit]
    block[offset] = len(data)
    block[offset + 1:offset + 1 + len(data)] = data


def amiga_date():
    now = time.time() - 252460800  # Seconds between 1970-01-01 and 1978-01-01.
    days, seconds = divmod(int(now), 86400)
    return days, seconds // 60, (seconds % 60) * 50


def stamp(block, offset):
    days, minutes, ticks = amiga_date()
    struct.pack_into(">III", block, offset, days, minutes, ticks)


def name_hash(name):
    # International-mode hash: case-insensitive over Latin-1 letters.
    def upper(character):
        code = ord(character)
        if ord("a") <= code <= ord("z") or (224 <= code <= 254 and code != 247):
            return code - 32
        return code
    value = len(name)
    for character in name:
        value = (value * 13 + upper(character)) & 0x7ff
    return value % HASH_TABLE_SIZE


def make_ffs(sectors, volume_name, file_name, contents):
    """A FFS (DOS\\3) filesystem with one file in its root directory."""
    reserved = 2
    image = bytearray(sectors * SECTOR)
    root = (sectors - 1 + reserved) // 2
    bits_per_bitmap = (LONGS - 1) * 32
    bitmap_count = -(-(sectors - reserved) // bits_per_bitmap)
    if bitmap_count > 25:
        raise SystemExit("Boot partition too large for a root block without bitmap extensions")
    bitmaps = list(range(root + 1, root + 1 + bitmap_count))
    header = bitmaps[-1] + 1

    data_count = -(-len(contents) // SECTOR)
    extension_count = max(0, -(-data_count // HASH_TABLE_SIZE) - 1)
    data_blocks, extension_blocks = [], []
    cursor = header + 1
    for index in range(data_count):
        if index and index % HASH_TABLE_SIZE == 0:
            extension_blocks.append(cursor)
            cursor += 1
        data_blocks.append(cursor)
        cursor += 1
    if cursor > sectors:
        raise SystemExit(f"{file_name} does not fit in the boot partition")
    used = {root, *bitmaps, header, *data_blocks, *extension_blocks}

    def block(number):
        return memoryview(image)[number * SECTOR:(number + 1) * SECTOR]

    boot = block(0)
    struct.pack_into(">II", boot, 0, DOS3, 0)
    struct.pack_into(">I", boot, 8, root)
    # The boot block checksum covers both reserved sectors, with end-around carry.
    total = 0
    for (value,) in struct.iter_unpack(">I", bytes(image[:reserved * SECTOR])):
        total += value
        if total > 0xffffffff:
            total = (total & 0xffffffff) + 1
    struct.pack_into(">I", boot, 4, ~total & 0xffffffff)

    root_block = block(root)
    put32(root_block, 0, T_HEADER)
    put32(root_block, 3, HASH_TABLE_SIZE)
    put32(root_block, 6 + name_hash(file_name), header)
    put32(root_block, LONGS - 50, 0xffffffff)  # Bitmap valid.
    for index, bitmap in enumerate(bitmaps):
        put32(root_block, LONGS - 49 + index, bitmap)
    stamp(root_block, SECTOR - 92)
    bcpl(root_block, SECTOR - 80, volume_name, 30)
    stamp(root_block, SECTOR - 40)
    stamp(root_block, SECTOR - 28)
    put32(root_block, LONGS - 1, ST_ROOT)
    set_checksum(root_block, 5)

    for index, bitmap in enumerate(bitmaps):
        bitmap_block = block(bitmap)
        for word in range(LONGS - 1):
            value = 0
            for bit in range(32):
                number = reserved + (index * (LONGS - 1) + word) * 32 + bit
                if number < sectors and number not in used:
                    value |= 1 << bit  # A set bit marks a free block.
            put32(bitmap_block, word + 1, value)
        set_checksum(bitmap_block, 0)

    lists = [header, *extension_blocks]
    for list_index, list_block_number in enumerate(lists):
        chunk = data_blocks[list_index * HASH_TABLE_SIZE:(list_index + 1) * HASH_TABLE_SIZE]
        list_block = block(list_block_number)
        put32(list_block, 0, T_HEADER if list_index == 0 else T_LIST)
        put32(list_block, 1, list_block_number)
        put32(list_block, 2, len(chunk))
        if chunk:
            put32(list_block, 4, chunk[0])
        for position, data_block in enumerate(chunk):
            put32(list_block, 6 + HASH_TABLE_SIZE - 1 - position, data_block)
        if list_index == 0:
            put32(list_block, LONGS - 47, len(contents))
            stamp(list_block, SECTOR - 92)
            bcpl(list_block, SECTOR - 80, file_name, 30)
            put32(list_block, LONGS - 3, root)
        else:
            put32(list_block, LONGS - 3, header)
        if list_index + 1 < len(lists):
            put32(list_block, LONGS - 2, lists[list_index + 1])
        put32(list_block, LONGS - 1, ST_FILE)
        set_checksum(list_block, 5)

    for index, data_block in enumerate(data_blocks):
        piece = contents[index * SECTOR:(index + 1) * SECTOR]
        image[data_block * SECTOR:data_block * SECTOR + len(piece)] = piece
    return bytes(image)


def rdb_block(identifier):
    block = bytearray(SECTOR)
    struct.pack_into(">IIiI", block, 0, identifier, 64, 0, 7)
    return block


def finish_rdb_block(block):
    struct.pack_into(">i", block, 8, 0)
    total = sum(struct.unpack(">64I", block[:256])) & 0xffffffff
    struct.pack_into(">I", block, 8, -total & 0xffffffff)
    return bytes(block)


def partition_block(next_block, name, first, count, dos_type, bootable, no_mount):
    block = rdb_block(0x50415254)  # "PART"
    struct.pack_into(">II", block, 16, next_block, (1 if bootable else 0) | (2 if no_mount else 0))
    bcpl(block, 36, name, 31)
    environment = (16, 128, 0, 1, 1, 1, 2, 0, 0, first, first + count - 1, 30, 0, 0x1fe00, 0x7ffffffe, 0, dos_type)
    struct.pack_into(">16Ii", block, 128, *environment[:15], environment[15], 0)
    struct.pack_into(">I", block, 128 + 16 * 4, dos_type)
    return finish_rdb_block(block)


def make_rdb(total_sectors, root_first, root_sectors):
    rdsk = rdb_block(0x5244534b)  # "RDSK"
    struct.pack_into(">IIIIIII", rdsk, 16, SECTOR, 0, 0xffffffff, 1, 0xffffffff, 0xffffffff, 0xffffffff)
    # One sector per cylinder keeps partition boundaries at sector granularity.
    struct.pack_into(">III", rdsk, 64, total_sectors, 1, 1)
    struct.pack_into(">IIIIIII", rdsk, 128, 0, FIRST_PARTITION_SECTOR - 1, FIRST_PARTITION_SECTOR, total_sectors - 1, 1, 0, 2)
    rdsk[160:168] = b"SERENITY"
    rdsk[168:184] = b"PEGASOS2 CARD   "
    return [
        finish_rdb_block(rdsk),
        partition_block(2, "SERBOOT", FIRST_PARTITION_SECTOR, BOOT_PARTITION_SECTORS, DOS3, True, False),
        partition_block(0xffffffff, "SERROOT", root_first, root_sectors, LNX0, False, True),
    ]


def boot_partition(loader):
    return make_ffs(BOOT_PARTITION_SECTORS, "SerenityBoot", "serenity.elf", loader.read_bytes())


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    card = sub.add_parser("card")
    card.add_argument("--loader", type=Path, required=True)
    card.add_argument("--root", type=Path, required=True, help="ext2 image for the root partition")
    card.add_argument("--output", type=Path, required=True)
    boot = sub.add_parser("boot-only")
    boot.add_argument("--loader", type=Path, required=True)
    boot.add_argument("--target", type=Path, required=True)
    args = parser.parse_args()

    if args.command == "card":
        root_bytes = args.root.stat().st_size
        if root_bytes % SECTOR:
            raise SystemExit("The root image size must be a multiple of 512 bytes")
        root_first = FIRST_PARTITION_SECTOR + BOOT_PARTITION_SECTORS
        root_sectors = root_bytes // SECTOR
        total_sectors = root_first + root_sectors
        with args.output.open("wb") as output:
            output.truncate(total_sectors * SECTOR)
            for index, block in enumerate(make_rdb(total_sectors, root_first, root_sectors)):
                output.seek(index * SECTOR)
                output.write(block)
            output.seek(FIRST_PARTITION_SECTOR * SECTOR)
            output.write(boot_partition(args.loader))
            output.seek(root_first * SECTOR)
            with args.root.open("rb") as root:
                while chunk := root.read(1 << 20):
                    output.write(chunk)
        print(f"{args.output}: {total_sectors * SECTOR // 1048576} MiB, boot partition 0 (serenity.elf), root partition 1")
    else:
        with args.target.open("r+b") as target:
            header = target.read(3 * SECTOR)
            boot_part = header[SECTOR:2 * SECTOR]
            if header[:4] != b"RDSK" or boot_part[:4] != b"PART" or boot_part[37:44] != b"SERBOOT" \
                    or struct.unpack_from(">I", boot_part, 128 + 9 * 4)[0] != FIRST_PARTITION_SECTOR:
                raise SystemExit("The target was not made by this tool")
            target.seek(FIRST_PARTITION_SECTOR * SECTOR)
            target.write(boot_partition(args.loader))
        print(f"{args.target}: boot partition rewritten")


if __name__ == "__main__":
    main()
