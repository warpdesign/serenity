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
    parser.add_argument("--readelf", required=True)
    parser.add_argument("--qemu", required=True)
    parser.add_argument("--kernel-loader", required=True)
    parser.add_argument("--user-loader", required=True)
    parser.add_argument("--libc", type=Path, required=True)
    parser.add_argument("--libsystem", type=Path, required=True)
    parser.add_argument("--memory", type=int, default=512)
    parser.add_argument("--cpu")
    args = parser.parse_args()

    mke2fs = shutil.which("mke2fs")
    if mke2fs is None:
        homebrew_mke2fs = Path("/opt/homebrew/opt/e2fsprogs/sbin/mke2fs")
        if homebrew_mke2fs.exists():
            mke2fs = str(homebrew_mke2fs)
    if mke2fs is None:
        raise SystemExit("mke2fs is required for the PowerPC RTTI runtime test")

    syscall_header = Path(__file__).parents[3] / "Kernel/API/Syscall.h"
    syscall_names = re.findall(r"^\s*S\((\w+),", syscall_header.read_text(), re.MULTILINE)
    dbgputstr_number = syscall_names.index("dbgputstr")

    with tempfile.TemporaryDirectory(prefix="serenity-powerpc-rtti-") as directory:
        work = Path(directory)
        executable = work / "init"
        source = Path(__file__).with_name("RTTIProbe.cpp")
        subprocess.run([
            args.compiler, "-m32", "-msecure-plt", "-fPIC", "-fno-stack-protector", "-fno-exceptions", "-O2",
            "-nostartfiles", "-nodefaultlibs", "-Wl,-e,_start", "-Wl,--dynamic-linker=/usr/lib/Loader.so",
            f"-DSYSCALL_DBGPUTSTR={dbgputstr_number}",
            "-L" + str(args.libc.resolve().parent), "-L" + str(args.libsystem.resolve().parent),
            "-Wl,-rpath-link," + str(args.libsystem.resolve().parent),
            str(source), "-lc", "-lsystem", "-lgcc", "-o", str(executable),
        ], check=True)
        symbols = subprocess.run([args.readelf, "-Ws", str(executable)], capture_output=True, text=True, check=True).stdout
        if not re.search(r"\bUND __dynamic_cast\b", symbols):
            raise SystemExit("RTTI probe did not retain its dynamic_cast call")

        root = work / "root"
        (root / "usr/lib").mkdir(parents=True)
        shutil.copy2(executable, root / "init")
        shutil.copy2(args.user_loader, root / "usr/lib/Loader.so")
        shutil.copy2(args.libc, root / "usr/lib/libc.so")
        shutil.copy2(args.libsystem, root / "usr/lib/libsystem.so")
        disk = work / "root.ext2"
        with disk.open("wb") as file:
            file.truncate(16 * 1024 * 1024)
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
        marker = b"PowerPC RTTI runtime passed"
        found = False
        try:
            deadline = time.monotonic() + 45
            while time.monotonic() < deadline and process.poll() is None:
                output = serial.read_bytes() if serial.exists() else b""
                if marker in output:
                    found = True
                    break
                if b"CRASH:" in output or b"KERNEL PANIC" in output:
                    break
                time.sleep(0.05)
        finally:
            process.terminate()
            _, errors = process.communicate(timeout=5)

        output = serial.read_bytes() if serial.exists() else b""
        if not found or b"KERNEL PANIC" in output or b"CRASH:" in output:
            raise SystemExit(
                "Pegasos 2 RTTI runtime probe failed:\n"
                + output.decode(errors="replace")[-3000:]
                + errors.decode(errors="replace")[-1000:]
            )
    print("Pegasos 2 C++ dynamic_cast runtime passed")


if __name__ == "__main__":
    main()
