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
    parser.add_argument("--kernel-loader", required=True)
    parser.add_argument("--user-loader", required=True)
    parser.add_argument("--memory", type=int, default=256)
    parser.add_argument("--cpu")
    args = parser.parse_args()

    mke2fs = shutil.which("mke2fs")
    if mke2fs is None:
        homebrew_mke2fs = Path("/opt/homebrew/opt/e2fsprogs/sbin/mke2fs")
        if homebrew_mke2fs.exists():
            mke2fs = str(homebrew_mke2fs)
    if mke2fs is None:
        raise SystemExit("mke2fs is required for the PowerPC dynamic loader test")

    syscall_header = Path(__file__).parents[3] / "Kernel/API/Syscall.h"
    syscall_names = re.findall(r"^\s*S\((\w+),", syscall_header.read_text(), re.MULTILINE)
    annotate_mapping_number = syscall_names.index("annotate_mapping")
    syscall_marker = f"syscall r0={annotate_mapping_number:016x}".encode()

    with tempfile.TemporaryDirectory(prefix="serenity-powerpc-dynamic-loader-") as directory:
        work = Path(directory)
        root = work / "root"
        root.mkdir()
        shutil.copyfile(args.user_loader, root / "init")
        disk = work / "root.ext2"
        with disk.open("wb") as file:
            file.truncate(16 * 1024 * 1024)
        subprocess.run([mke2fs, "-q", "-F", "-t", "ext2", "-O", "none", "-b", "1024", "-d", str(root), str(disk)], check=True)
        with disk.open("r+b") as file:
            file.seek(510)
            file.write(b"\x55\xaa")

        serial = work / "serial.log"
        interrupts = work / "interrupts.log"
        command = [
            args.qemu, "-M", "pegasos2", "-m", str(args.memory), "-kernel", args.kernel_loader,
            "-append", "root=lun3:0:0",
            "-drive", f"if=none,format=raw,id=virtio_disk,file={disk}",
            "-device", "virtio-blk-pci,drive=virtio_disk",
            "-object", "rng-random,id=rng0,filename=/dev/urandom",
            "-device", "virtio-rng-pci,rng=rng0",
            "-display", "none", "-serial", f"file:{serial}",
            "-monitor", "none", "-no-reboot", "-icount", "shift=auto",
            "-d", "int", "-D", str(interrupts),
        ]
        if args.cpu:
            command.extend(("-cpu", args.cpu))
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        found = False
        trace_position = 0
        trace_tail = b""
        try:
            deadline = time.monotonic() + 12
            while time.monotonic() < deadline and process.poll() is None:
                if interrupts.exists():
                    with interrupts.open("rb") as trace:
                        trace.seek(trace_position)
                        new_trace = trace.read()
                        trace_position = trace.tell()
                    combined_trace = trace_tail + new_trace
                    trace_tail = combined_trace[-len(syscall_marker):]
                    if syscall_marker in combined_trace:
                        found = True
                        break
                    if b"HV_EMU" in combined_trace:
                        break
                if serial.exists() and b"CRASH:" in serial.read_bytes():
                    break
                time.sleep(0.05)
        finally:
            process.terminate()
            _, errors = process.communicate(timeout=5)

        output = serial.read_bytes() if serial.exists() else b""
        if not found or b"Ext2FS: Mount successful" not in output or b"Running first user process: /init" not in output or b"KERNEL PANIC" in output:
            raise SystemExit(
                f"Pegasos 2 dynamic loader did not reach its first mapping syscall: found={found}\n"
                + output.decode(errors="replace")[-3000:]
                + errors.decode(errors="replace")[-1000:]
            )
    print("Pegasos 2 dynamic loader self-relocation passed")


if __name__ == "__main__":
    main()
