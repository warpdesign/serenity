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
    parser.add_argument("--kernel-loader", required=True)
    parser.add_argument("--user-loader", required=True)
    parser.add_argument("--program", required=True)
    parser.add_argument("--library", action="append", required=True)
    parser.add_argument("--memory", type=int, default=512)
    parser.add_argument("--cpu")
    args = parser.parse_args()

    mke2fs = shutil.which("mke2fs")
    if mke2fs is None:
        homebrew_mke2fs = Path("/opt/homebrew/opt/e2fsprogs/sbin/mke2fs")
        if homebrew_mke2fs.exists():
            mke2fs = str(homebrew_mke2fs)
    if mke2fs is None:
        raise SystemExit("mke2fs is required for the PowerPC program mapping test")

    with tempfile.TemporaryDirectory(prefix="serenity-powerpc-program-mapping-") as directory:
        work = Path(directory)
        root = work / "root"
        (root / "usr/lib").mkdir(parents=True)
        shutil.copyfile(args.program, root / "init")
        shutil.copyfile(args.user_loader, root / "usr/lib/Loader.so")
        for library in args.library:
            shutil.copyfile(library, root / "usr/lib" / Path(library).name)
        disk = work / "root.ext2"
        with disk.open("wb") as file:
            file.truncate(64 * 1024 * 1024)
        subprocess.run([mke2fs, "-q", "-F", "-t", "ext2", "-O", "none", "-b", "1024", "-d", str(root), str(disk)], check=True)
        with disk.open("r+b") as file:
            file.seek(510)
            file.write(b"\x55\xaa")

        serial = work / "serial.log"
        command = [
            args.qemu, "-M", "pegasos2", "-m", str(args.memory), "-kernel", args.kernel_loader,
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
        marker = b"DynamicObject: DYNAMIC tag handling not implemented for DT_LOPROC (1879048192) in /usr/lib/libunicode.so.serenity"
        found = False
        try:
            deadline = time.monotonic() + 35
            while time.monotonic() < deadline and process.poll() is None:
                output = serial.read_bytes() if serial.exists() else b""
                if marker in output:
                    found = True
                    break
                if b"KERNEL PANIC" in output:
                    break
                time.sleep(0.05)
        finally:
            process.terminate()
            _, errors = process.communicate(timeout=5)

        output = serial.read_bytes() if serial.exists() else b""
        if not found or b"Running first user process: /init" not in output or b"KERNEL PANIC" in output:
            raise SystemExit(
                "Pegasos 2 dynamic loader did not map /init and its dependencies:\n"
                + output.decode(errors="replace")[-3000:]
                + errors.decode(errors="replace")[-1000:]
            )
    print("Pegasos 2 dynamic loader program and dependency mapping passed")


if __name__ == "__main__":
    main()
