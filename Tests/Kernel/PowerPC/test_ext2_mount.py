#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", required=True)
    parser.add_argument("--loader", required=True)
    parser.add_argument("--memory", type=int, default=512)
    parser.add_argument("--cpu")
    args = parser.parse_args()

    mke2fs = shutil.which("mke2fs")
    if mke2fs is None:
        homebrew_mke2fs = Path("/opt/homebrew/opt/e2fsprogs/sbin/mke2fs")
        if homebrew_mke2fs.exists():
            mke2fs = str(homebrew_mke2fs)
    if mke2fs is None:
        raise SystemExit("mke2fs is required for the PowerPC ext2 mount test")

    with tempfile.TemporaryDirectory(prefix="serenity-powerpc-ext2-") as directory:
        work = Path(directory)
        disk = work / "root.ext2"
        with disk.open("wb") as file:
            file.truncate(16 * 1024 * 1024)
        subprocess.run([mke2fs, "-q", "-F", "-t", "ext2", "-O", "none", "-b", "1024", str(disk)], check=True)
        with disk.open("r+b") as file:
            file.seek(510)
            file.write(b"\x55\xaa")

        serial = work / "serial.log"
        command = [
            args.qemu,
            "-M", "pegasos2",
            "-m", str(args.memory),
            "-kernel", args.loader,
            "-append", "root=lun3:0:0",
            "-drive", f"if=none,format=raw,id=virtio_disk,file={disk}",
            "-device", "virtio-blk-pci,drive=virtio_disk",
            "-display", "none",
            "-serial", f"file:{serial}",
            "-monitor", "none",
            "-no-reboot",
            "-icount", "shift=auto",
        ]
        if args.cpu:
            command.extend(("-cpu", args.cpu))
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        try:
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline and process.poll() is None:
                output = serial.read_bytes() if serial.exists() else b""
                if b"Running first user process: /init" in output or b"KERNEL PANIC" in output:
                    break
                time.sleep(0.05)
        finally:
            process.terminate()
            _, errors = process.communicate(timeout=5)

        output = serial.read_bytes() if serial.exists() else b""
        expected = (
            b"Ext2FS: Mount successful",
            b"Running first user process: /init",
        )
        missing = [marker for marker in expected if marker not in output]
        if b"KERNEL PANIC" in output.split(b"Running first user process: /init", 1)[0]:
            missing.append(b"no kernel panic before init")
        if missing:
            raise SystemExit(
                f"Pegasos 2 ext2 mount missed {missing!r}:\n"
                + output.decode(errors="replace")[-3000:]
                + errors.decode(errors="replace")[-1000:]
            )
    print("Pegasos 2 ext2 mount passed")


if __name__ == "__main__":
    main()
