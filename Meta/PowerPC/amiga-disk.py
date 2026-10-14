#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

"""Read-only access to Amiga RDB disks with OFS/FFS partitions, as booted by Pegasos SmartFirmware.

    amiga-disk.py partitions DISK
    amiga-disk.py ls DISK PARTITION [PATH]
    amiga-disk.py extract DISK PARTITION PATH OUTPUT

PARTITION is the zero-based RDB partition number SmartFirmware uses (`boot ide:0 file`) or its
drive name. Classic (DOS\\0-\\5) and long file name (DOS\\6, DOS\\7) variants are supported, with
any number of sectors per block. The disk is never written. Disks from the internet are untrusted:
run this with `python3 -I` and keep its output in a directory of its own.
"""

import struct
import sys

SECTOR = 512


def be32(data, offset):
    return struct.unpack(">I", data[offset:offset + 4])[0]


def sbe32(data, offset):
    return struct.unpack(">i", data[offset:offset + 4])[0]


class Disk:
    def __init__(self, path):
        self.file = open(path, "rb")

    def read(self, sector, count=1):
        self.file.seek(sector * SECTOR)
        return self.file.read(count * SECTOR)

    def partitions(self):
        for sector in range(16):
            rdsk = self.read(sector)
            if rdsk[:4] == b"RDSK":
                break
        else:
            raise SystemExit("No RDB (RDSK block) in the first 16 sectors")
        partitions = []
        pointer = be32(rdsk, 28)
        while pointer != 0xffffffff and len(partitions) < 128:
            part = self.read(pointer)
            if part[:4] != b"PART":
                raise SystemExit(f"Corrupt partition list at sector {pointer}")
            environment = 128
            surfaces, sectors_per_block, blocks_per_track = be32(part, environment + 12), be32(part, environment + 16), be32(part, environment + 20)
            low, high = be32(part, environment + 36), be32(part, environment + 40)
            partitions.append({
                "name": part[37:37 + part[36]].decode("latin1"),
                "first_sector": low * surfaces * blocks_per_track,
                "sectors": (high - low + 1) * surfaces * blocks_per_track,
                "block_size": be32(part, environment + 4) * 4 * max(sectors_per_block, 1),
                "reserved": be32(part, environment + 24),
                "dos_type": part[environment + 64:environment + 68],
                "boot_priority": sbe32(part, environment + 60),
            })
            pointer = be32(part, 16)
        return partitions


class FileSystem:
    def __init__(self, disk, partition):
        self.disk = disk
        self.partition = partition
        self.block_size = partition["block_size"]
        dos_type = partition["dos_type"]
        if dos_type[:3] != b"DOS" or dos_type[3] > 7:
            raise SystemExit(f"Unsupported filesystem {dos_type!r}")
        self.fast = dos_type[3] & 1 == 1
        self.long_names = dos_type[3] >= 6
        blocks = partition["sectors"] * SECTOR // self.block_size
        boot = self.block(0)
        self.root = be32(boot, 8) or (blocks - 1 + partition["reserved"]) // 2

    def block(self, number):
        sectors = self.block_size // SECTOR
        return self.disk.read(self.partition["first_sector"] + number * sectors, sectors)

    def name(self, header):
        offset = self.block_size - (0xb8 if self.long_names else 80)
        return header[offset + 1:offset + 1 + header[offset]].decode("latin1", "replace")

    def entries(self, directory):
        header = self.block(directory)
        table_size = self.block_size // 4 - 56
        for index in range(table_size):
            pointer = be32(header, 24 + 4 * index)
            while pointer:
                entry = self.block(pointer)
                kind = {2: "dir", 1: "root", -3: "file", 3: "link", -4: "link"}.get(sbe32(entry, self.block_size - 4), "?")
                yield self.name(entry), kind, be32(entry, self.block_size - 188), pointer
                pointer = be32(entry, self.block_size - 16)

    def lookup(self, path):
        block = self.root
        for component in (part for part in path.split("/") if part):
            matches = [entry for entry in self.entries(block) if entry[0].lower() == component.lower()]
            if not matches:
                raise SystemExit(f"{path}: not found")
            block = matches[0][3]
        return block

    def read_file(self, header_block):
        header = self.block(header_block)
        size = be32(header, self.block_size - 188)
        table_size = self.block_size // 4 - 56
        data_blocks = []
        current = header
        while True:
            # Data block pointers are stored from the end of the table backwards.
            for index in range(table_size - 1, -1, -1):
                pointer = be32(current, 24 + 4 * index)
                if pointer == 0:
                    break
                data_blocks.append(pointer)
            extension = be32(current, self.block_size - 8)
            if extension == 0:
                break
            current = self.block(extension)
        chunks = []
        for pointer in data_blocks:
            data = self.block(pointer)
            # OFS data blocks start with a 24-byte header holding the payload size.
            chunks.append(data if self.fast else data[24:24 + be32(data, 12)])
        return b"".join(chunks)[:size]


def select(partitions, key):
    if key.isdigit():
        return partitions[int(key)]
    for partition in partitions:
        if partition["name"] == key:
            return partition
    raise SystemExit(f"No partition {key!r}")


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    command, disk = sys.argv[1], Disk(sys.argv[2])
    partitions = disk.partitions()
    if command == "partitions":
        for index, partition in enumerate(partitions):
            print(f"{index}: {partition['name']} {partition['dos_type']!r} sectors {partition['first_sector']}+{partition['sectors']} "
                  f"({partition['sectors'] * SECTOR // 1048576} MiB), block size {partition['block_size']}, boot priority {partition['boot_priority']}")
    elif command == "ls":
        filesystem = FileSystem(disk, select(partitions, sys.argv[3]))
        directory = filesystem.lookup(sys.argv[4] if len(sys.argv) > 4 else "")
        for name, kind, size, _ in sorted(filesystem.entries(directory), key=lambda entry: entry[0].lower()):
            print(f"{kind:5} {size if kind == 'file' else '':>10} {name}")
    elif command == "extract":
        filesystem = FileSystem(disk, select(partitions, sys.argv[3]))
        data = filesystem.read_file(filesystem.lookup(sys.argv[4]))
        with open(sys.argv[5], "wb") as output:
            output.write(data)
        print(f"extracted {len(data)} bytes")
    else:
        raise SystemExit(__doc__)


if __name__ == "__main__":
    main()
