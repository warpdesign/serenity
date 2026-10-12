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
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--qemu", required=True)
    parser.add_argument("--kernel-loader", required=True)
    parser.add_argument("--user-loader", required=True)
    parser.add_argument("--libc", type=Path, required=True)
    parser.add_argument("--libsystem", type=Path, required=True)
    parser.add_argument("--memory", type=int, default=512)
    parser.add_argument("--cpu", required=True)
    args = parser.parse_args()

    mke2fs = shutil.which("mke2fs")
    if mke2fs is None:
        homebrew_mke2fs = Path("/opt/homebrew/opt/e2fsprogs/sbin/mke2fs")
        if homebrew_mke2fs.exists():
            mke2fs = str(homebrew_mke2fs)
    if mke2fs is None:
        raise SystemExit("mke2fs is required for the PowerPC block latency test")

    source = Path(__file__).parents[3]
    syscall_names = re.findall(r"^\s*S\((\w+),", (source / "Kernel/API/Syscall.h").read_text(), re.MULTILINE)
    dbgputstr_number = syscall_names.index("dbgputstr")

    with tempfile.TemporaryDirectory(prefix="serenity-powerpc-block-latency-") as directory:
        work = Path(directory)
        executable = work / "init"
        subprocess.run([
            args.compiler, "-m32", "-msecure-plt", "-fPIC", "-fno-stack-protector", "-O2",
            "-nostartfiles", "-nodefaultlibs", "-Wl,-e,_start", "-Wl,--dynamic-linker=/usr/lib/Loader.so",
            f"-DSYSCALL_DBGPUTSTR={dbgputstr_number}", "-I" + str(source / "Userland/Libraries/LibC"),
            "-L" + str(args.libc.resolve().parent), "-L" + str(args.libsystem.resolve().parent),
            str(Path(__file__).parent / "Fixtures/VirtIOBlockLatency.c"), "-lc", "-lsystem", "-lgcc", "-o", str(executable),
        ], check=True)

        root = work / "root"
        (root / "usr/lib").mkdir(parents=True)
        shutil.copy2(executable, root / "init")
        shutil.copy2(args.user_loader, root / "usr/lib/Loader.so")
        shutil.copy2(args.libc, root / "usr/lib/libc.so")
        shutil.copy2(args.libsystem, root / "usr/lib/libsystem.so")
        with (root / "data").open("wb") as file:
            for block in range(97):
                file.write(bytes([block]) + bytes(4095))
        disk = work / "root.ext2"
        with disk.open("wb") as file:
            file.truncate(16 * 1024 * 1024)
        subprocess.run([mke2fs, "-q", "-F", "-t", "ext2", "-O", "none", "-b", "4096", "-d", str(root), str(disk)], check=True)
        with disk.open("r+b") as file:
            file.seek(510)
            file.write(b"\x55\xaa")

        serial = work / "serial.log"
        command = [
            args.qemu, "-M", "pegasos2", "-cpu", args.cpu, "-m", str(args.memory), "-kernel", args.kernel_loader,
            "-append", "root=lun3:0:0", "-drive", f"if=none,format=raw,id=virtio_disk,file={disk}",
            "-device", "virtio-blk-pci,drive=virtio_disk", "-object", "rng-random,id=rng0,filename=/dev/urandom",
            "-device", "virtio-rng-pci,rng=rng0", "-display", "none", "-serial", f"file:{serial}",
            "-monitor", "none", "-no-reboot",
        ]
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        started_at = None
        ended_at = None
        try:
            deadline = time.monotonic() + 20
            while time.monotonic() < deadline and process.poll() is None:
                output = serial.read_text(errors="replace") if serial.exists() else ""
                if started_at is None and "PowerPC block reads begin" in output:
                    started_at = time.monotonic()
                if "PowerPC block reads ended" in output:
                    ended_at = time.monotonic()
                    break
                if "KERNEL PANIC" in output or "CRASH:" in output:
                    break
                time.sleep(0.01)
        finally:
            process.terminate()
            _, errors = process.communicate(timeout=5)

        output = serial.read_text(errors="replace") if serial.exists() else ""
        elapsed = ended_at - started_at if started_at is not None and ended_at is not None else None
        if elapsed is None or elapsed > 2 or "KERNEL PANIC" in output or "CRASH:" in output:
            raise SystemExit(f"Pegasos 2 read latency failed (97 reads took {elapsed}s):\n"
                             + output[-3000:] + errors.decode(errors="replace")[-1000:])
    print(f"Pegasos 2 completed 97 nonsequential 4 KiB file reads in {elapsed:.3f}s")


if __name__ == "__main__":
    main()
