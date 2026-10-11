#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

import argparse
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", required=True)
    parser.add_argument("--loader", required=True)
    parser.add_argument("--cross-compile", required=True)
    parser.add_argument("--memory", type=int, default=512)
    parser.add_argument("--cpu")
    args = parser.parse_args()

    mke2fs = shutil.which("mke2fs")
    if mke2fs is None:
        homebrew_mke2fs = Path("/opt/homebrew/opt/e2fsprogs/sbin/mke2fs")
        if homebrew_mke2fs.exists():
            mke2fs = str(homebrew_mke2fs)
    if mke2fs is None:
        raise SystemExit("mke2fs is required for the PowerPC mapping failure test")

    syscall_header = Path(__file__).parents[3] / "Kernel/API/Syscall.h"
    syscall_names = re.findall(r"^\s*S\((\w+),", syscall_header.read_text(), re.MULTILINE)

    with tempfile.TemporaryDirectory(prefix="serenity-powerpc-mapping-failure-") as directory:
        work = Path(directory)
        object_file = work / "mapping-failure.o"
        executable = work / "init"
        fixtures = Path(__file__).parent / "Fixtures"
        subprocess.run([
            args.cross_compile + "gcc", "-m32", "-msdata=none", "-ffreestanding", "-fno-pic", "-fno-pie",
            f"-DSYSCALL_MMAP={syscall_names.index('mmap')}",
            f"-DSYSCALL_DBGPUTSTR={syscall_names.index('dbgputstr')}",
            "-c", str(fixtures / "MappingFailure.S"), "-o", str(object_file),
        ], check=True)
        subprocess.run([
            args.cross_compile + "ld", "-m", "elf32ppcserenity", "-shared", "--no-dynamic-linker",
            "-T", str(fixtures / "UserLoop.ld"), "--entry=_start", str(object_file), "-o", str(executable),
        ], check=True)

        root = work / "root"
        root.mkdir()
        shutil.copyfile(executable, root / "init")
        disk = work / "root.ext2"
        with disk.open("wb") as file:
            file.truncate(16 * 1024 * 1024)
        subprocess.run([mke2fs, "-q", "-F", "-t", "ext2", "-O", "none", "-b", "1024", "-d", str(root), str(disk)], check=True)
        with disk.open("r+b") as file:
            file.seek(510)
            file.write(b"\x55\xaa")

        serial = work / "serial.log"
        command = [
            args.qemu, "-M", "pegasos2", "-m", str(args.memory), "-kernel", args.loader,
            "-append", "root=lun3:0:0",
            "-drive", f"if=none,format=raw,id=virtio_disk,file={disk}",
            "-device", "virtio-blk-pci,drive=virtio_disk",
            "-object", "rng-random,id=rng0,filename=/dev/urandom",
            "-device", "virtio-rng-pci,rng=rng0",
            "-display", "none", "-serial", f"file:{serial}",
            "-monitor", "none", "-no-reboot", "-icount", "shift=auto",
        ]
        if args.cpu:
            command.extend(("-cpu", args.cpu))
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        try:
            deadline = time.monotonic() + 30
            while time.monotonic() < deadline and process.poll() is None:
                output = serial.read_bytes() if serial.exists() else b""
                if b"PPC mapping failure recovered" in output or b"KERNEL PANIC" in output:
                    break
                time.sleep(0.05)
        finally:
            process.terminate()
            _, errors = process.communicate(timeout=5)

        output = serial.read_bytes() if serial.exists() else b""
        if b"PPC mapping failure recovered" not in output or b"KERNEL PANIC" in output:
            raise SystemExit(
                "Pegasos 2 failed to recover from a partially mapped region:\n"
                + output.decode(errors="replace")[-3000:]
                + errors.decode(errors="replace")[-1000:]
            )
    print("Pegasos 2 mapping failure recovery passed")


if __name__ == "__main__":
    main()
