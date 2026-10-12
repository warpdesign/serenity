#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

import argparse
from pathlib import Path
import re
import socket
import struct
import subprocess
import sys
import tempfile
import time

sys.dont_write_bytecode = True
import test_window_server as graphical_boot

HPT_BYTES = 4 * 1024 * 1024


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", required=True)
    parser.add_argument("--loader", type=Path, required=True)
    parser.add_argument("--user-loader", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--cpu", default="750")
    parser.add_argument("--memory", type=int, default=512)
    args = parser.parse_args()

    source = Path(__file__).parents[3]
    with tempfile.TemporaryDirectory(prefix="powerpc-virtio-gpu-footprint-") as directory:
        work = Path(directory)
        root = graphical_boot.create_root(source, args.build_dir, args.user_loader, work)
        disk = graphical_boot.create_disk(graphical_boot.find_mke2fs(), root, work)
        serial = work / "serial.log"
        monitor_path = work / "monitor.sock"
        snapshot = work / "hpt.bin"
        command = [
            args.qemu, "-M", "pegasos2", "-cpu", args.cpu, "-m", str(args.memory),
            "-vga", "none", "-device", "virtio-gpu-pci", "-kernel", str(args.loader),
            "-append", "root=lun3:0:0", "-drive", f"if=none,format=raw,id=virtio_disk,file={disk}",
            "-device", "virtio-blk-pci,drive=virtio_disk",
            "-object", "rng-random,id=rng0,filename=/dev/urandom", "-device", "virtio-rng-pci,rng=rng0",
            "-display", "none", "-serial", f"file:{serial}",
            "-monitor", f"unix:{monitor_path},server=on,wait=off", "-no-reboot",
        ]
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        try:
            deadline = time.monotonic() + 30
            while not monitor_path.exists() and process.poll() is None and time.monotonic() < deadline:
                time.sleep(0.05)
            with socket.socket(socket.AF_UNIX) as monitor:
                monitor.settimeout(8)
                monitor.connect(str(monitor_path))
                greeting = b""
                while not greeting.endswith(b"(qemu) "):
                    greeting += monitor.recv(16384)
                while time.monotonic() < deadline and process.poll() is None:
                    output = serial.read_bytes() if serial.exists() else b""
                    if b"Running first user process: /init" in output or b"KERNEL PANIC" in output:
                        break
                    time.sleep(0.05)
                output = serial.read_bytes() if serial.exists() else b""
                if b"VirtIOGPU: Found" not in output or b"Running first user process: /init" not in output:
                    raise RuntimeError("Pegasos 2 did not initialize VirtIO GPU and reach /init")
                hpt_ranges = re.findall(rb"Firmware Reserved range @ P([0-9a-f]{16}) - P[0-9a-f]{16} \(size 0x400000\)", output)
                if len(hpt_ranges) != 1:
                    raise RuntimeError("Could not identify the 4 MiB PowerPC hash page table")
                hpt_address = int(hpt_ranges[0], 16)
                reply = graphical_boot.monitor_command(monitor, f'pmemsave {hpt_address:#x} {HPT_BYTES:#x} "{snapshot}"')
                if b"Error" in reply or b"invalid" in reply or not snapshot.exists():
                    raise RuntimeError("Could not capture the PowerPC hash page table")
        finally:
            process.terminate()
            _, errors = process.communicate(timeout=5)

        entries = snapshot.read_bytes()
        if len(entries) != HPT_BYTES:
            raise SystemExit("QEMU returned a truncated PowerPC hash page table")
        kernel_entries = sum(bool(upper & 0x80000000) and ((upper >> 7) & 0x00ffffff) < 0x100
                             for upper, _ in struct.iter_unpack(">II", entries))
        if kernel_entries > 50_000:
            raise SystemExit(f"VirtIO GPU boot used {kernel_entries} kernel hash entries before /init; expected at most 50000\n"
                             + errors.decode(errors="replace")[-1000:])
        print(f"Pegasos 2 VirtIO GPU boot used {kernel_entries} kernel hash entries before /init")


if __name__ == "__main__":
    main()
