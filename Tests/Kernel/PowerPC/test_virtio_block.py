#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

import argparse
from pathlib import Path
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", required=True)
    parser.add_argument("--loader", required=True)
    parser.add_argument("--memory", type=int, default=256)
    parser.add_argument("--cpu")
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="serenity-powerpc-virtio-block-") as directory:
        work = Path(directory)
        disk = work / "blank.raw"
        with disk.open("wb") as file:
            file.truncate(16 * 1024 * 1024)
        serial = work / "serial.log"
        interrupts = work / "interrupts.log"
        command = [
            args.qemu,
            "-M", "pegasos2",
            "-m", str(args.memory),
            "-kernel", args.loader,
            "-drive", f"if=none,format=raw,id=virtio_disk,file={disk}",
            "-device", "virtio-blk-pci,drive=virtio_disk",
            "-display", "none",
            "-serial", f"file:{serial}",
            "-monitor", "none",
            "-no-reboot",
            "-d", "int",
            "-D", str(interrupts),
        ]
        if args.cpu:
            command.extend(("-cpu", args.cpu))
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        try:
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline and process.poll() is None:
                serial_output = serial.read_bytes() if serial.exists() else b""
                if b"Master Boot Record: invalid signature" in serial_output:
                    break
                if interrupts.exists() and interrupts.stat().st_size > 2 * 1024 * 1024:
                    break
                time.sleep(0.05)
        finally:
            process.terminate()
            _, errors = process.communicate(timeout=5)

        serial_output = serial.read_bytes() if serial.exists() else b""
        interrupt_output = interrupts.read_bytes() if interrupts.exists() else b""
        expected = (
            b"VirtIOBlockDevice: Found @ PCI",
            b"Master Boot Record: invalid signature",
        )
        missing = [marker for marker in expected if marker not in serial_output]
        if b"EXTERNAL (4)" not in interrupt_output:
            missing.append(b"external interrupt")
        if b"KERNEL PANIC" in serial_output.split(b"Master Boot Record: invalid signature", 1)[0]:
            missing.append(b"no kernel panic during disk read")
        if missing:
            raise SystemExit(
                f"Pegasos 2 VirtIO block read missed {missing!r}:\n"
                + serial_output.decode(errors="replace")[-3000:]
                + errors.decode(errors="replace")[-1000:]
            )
    print("Pegasos 2 VirtIO block read passed")


if __name__ == "__main__":
    main()
