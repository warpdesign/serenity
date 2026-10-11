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

    with tempfile.TemporaryDirectory(prefix="serenity-powerpc-kernel-entry-") as directory:
        serial = Path(directory) / "serial.log"
        trace = Path(directory) / "vof.log"
        command = [
            args.qemu,
            "-M", "pegasos2",
            "-m", str(args.memory),
            "-kernel", args.loader,
            "-append", "serenity-loader-enter-kernel",
            "-display", "none",
            "-serial", f"file:{serial}",
            "-monitor", "none",
            "-no-reboot",
            "-d", "trace:vof_write",
            "-D", str(trace),
        ]
        if args.cpu:
            command.extend(("-cpu", args.cpu))
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        try:
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline and process.poll() is None:
                serial_output = serial.read_bytes() if serial.exists() else b""
                if b"Boot method: FirmwareMemoryMap" in serial_output:
                    break
                time.sleep(0.05)
        finally:
            process.terminate()
            _, errors = process.communicate(timeout=5)
        serial_output = serial.read_bytes() if serial.exists() else b""
        trace_output = trace.read_bytes() if trace.exists() else b""
        expected_serial = (
            b"Loading kernel symbol table",
            b"MM: PowerPC physical-page entries",
            b"Starting SerenityOS...",
            b"Kernel Commandline: serenity-loader-enter-kernel serial_debug",
            b"Boot method: FirmwareMemoryMap",
        )
        expected_firmware = (
            b"SerenityOS PowerPC loader: boot handoff validated",
            b"SerenityOS PowerPC loader: entering kernel",
        )
        missing = [marker for marker in expected_serial if marker not in serial_output]
        missing += [marker for marker in expected_firmware if marker not in trace_output]
        if missing:
            raise SystemExit(
                f"Pegasos 2 kernel entry missed {missing!r}:\n"
                + serial_output.decode(errors="replace")
                + trace_output.decode(errors="replace")[-2500:]
                + errors.decode(errors="replace")
            )
    print("Pegasos 2 Serenity kernel entry passed")


if __name__ == "__main__":
    main()
