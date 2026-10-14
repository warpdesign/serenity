#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

"""Turn a qemu-tbprof.c profile into per-object and per-function instruction counts.

Kernel blocks are symbolized against Build/powerpc/Kernel/Kernel using the kernel's runtime
base (the PowerPC loader prints it as "runtime base"; 0xc1c00000 in QEMU so far).

Userland blocks need the process memory maps, because libraries are loaded at random addresses.
Dump them from the guest while the profiled processes run, e.g. as root:
    cat /proc/$(pidof -s WindowServer)/vm > /root/ws.vm
then copy the files out of the disk image (debugfs -R "dump /root/ws.vm ws.vm" disk.ext2) and pass
    --maps WindowServer=ws.vm --maps PixelPaint=pp.vm
"""

import argparse
import collections
import json
import subprocess
from pathlib import Path


def text_regions(vm_file, process):
    objects = {}
    texts = []
    for region in json.loads(Path(vm_file).read_text()):
        name = region["name"].split(":")[0]
        if name.startswith("/"):
            objects[name] = min(objects.get(name, 1 << 40), region["address"])
        if region["executable"]:
            texts.append((region["address"], region["address"] + region["size"], name, process))
    return [(low, high, name, process, objects.get(name, low)) for low, high, name, process in texts]


def describe(addr2line, binary, offset):
    frames = subprocess.run([addr2line, "-f", "-C", "-i", "-s", "-e", str(binary), hex(offset)],
                            capture_output=True, text=True).stdout.splitlines()
    names = [name.split("(")[0] for name in frames[0::2]]
    lines = frames[1::2]
    if not names:
        return "??"
    inlined = " / ".join(name.split("::")[-1] for name in names[:-1][:3])
    return names[-1] + (f"  <=  {inlined}" if inlined else "") + (f"  @{lines[0]}" if lines else "")


def main():
    source = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("profile", type=Path)
    parser.add_argument("--build-dir", type=Path, default=source / "Build/powerpc")
    parser.add_argument("--toolchain-prefix", default=str(source / "Toolchain/Local/powerpc/bin/powerpc-serenity-"))
    parser.add_argument("--kernel-base", type=lambda value: int(value, 0), default=0xc1c00000)
    parser.add_argument("--maps", action="append", default=[], metavar="PROCESS=VMFILE")
    parser.add_argument("--top", type=int, default=40)
    parser.add_argument("--blocks-per-object", type=int, default=400)
    args = parser.parse_args()

    addr2line = args.toolchain_prefix + "addr2line"
    binaries = {}
    for path in (args.build_dir / "Userland").rglob("*"):
        if path.is_file() and (path.suffix in (".serenity", ".so") or path.parent.name == path.name):
            binaries.setdefault(path.name, path)
    regions = []
    for entry in args.maps:
        process, vm_file = entry.split("=", 1)
        regions += text_regions(vm_file, process)

    rows = [(int(address, 16), int(instructions), int(executions))
            for address, instructions, executions in (line.split() for line in args.profile.read_text().splitlines())]
    total = sum(instructions * executions for _, instructions, executions in rows)

    by_object = collections.Counter()
    pending = collections.defaultdict(list)
    for address, instructions, executions in rows:
        cost = instructions * executions
        if address >= 0xc0000000:
            by_object["kernel"] += cost
            pending[("kernel", args.build_dir / "Kernel/Kernel")].append((address - args.kernel_base, cost))
            continue
        match = next((region for region in regions if region[0] <= address < region[1]), None)
        if match is None:
            by_object["unmapped userland"] += cost
            continue
        _, _, name, process, base = match
        key = f"{process}: {name}"
        by_object[key] += cost
        if binary := binaries.get(Path(name).name):
            pending[(key, binary)].append((address - base, cost))

    print(f"total {total / 1e6:.1f}M guest instructions")
    for name, cost in by_object.most_common(15):
        print(f"{cost * 100 / total:5.1f}%  {name}")

    functions = collections.Counter()
    for (key, binary), entries in pending.items():
        entries.sort(key=lambda entry: -entry[1])
        for offset, cost in entries[:args.blocks_per_object]:
            functions[f"{key.split(':')[0]:12} {binary.name:22} {describe(addr2line, binary, offset)[:160]}"] += cost
    print()
    for name, cost in functions.most_common(args.top):
        print(f"{cost * 100 / total:5.1f}%  {name}")


if __name__ == "__main__":
    main()
