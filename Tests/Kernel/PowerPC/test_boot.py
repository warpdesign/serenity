#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

import argparse
from pathlib import Path
import subprocess
import sys
import tempfile
import time


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", required=True)
    parser.add_argument("--kernel", required=True)
    parser.add_argument("--memory", type=int, default=256)
    parser.add_argument("--cpu")
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="serenity-pegasos2-") as directory:
        trace_path = Path(directory) / "vof.log"
        serial_path = Path(directory) / "serial.log"
        command = [
            args.qemu,
            "-M", "pegasos2",
            "-m", str(args.memory),
            "-kernel", args.kernel,
            "-display", "none",
            "-serial", f"file:{serial_path}",
            "-monitor", "none",
            "-no-reboot",
            "-d", "trace:vof_getprop,trace:vof_write",
            "-D", str(trace_path),
        ]
        if args.cpu:
            command.extend(("-cpu", args.cpu))
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        try:
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline and process.poll() is None:
                output = trace_path.read_bytes() if trace_path.exists() else b""
                serial_output = serial_path.read_bytes() if serial_path.exists() else b""
                if b'vof_write' in output and b'"Pegasos2"' in output and b"SerenityOS PowerPC: setjmp roundtrip passed" in serial_output and b"SerenityOS PowerPC: 32-bit kernel base 0xc0000000" in serial_output and f"SerenityOS PowerPC: RAM {args.memory} MiB".encode() in serial_output:
                    break
                time.sleep(0.05)
        finally:
            process.terminate()
            _, error_output = process.communicate(timeout=5)
        output = trace_path.read_bytes() if trace_path.exists() else b""
        serial_output = serial_path.read_bytes() if serial_path.exists() else b""

    expected = (
        (b"vof_write", b"SerenityOS PowerPC: Open Firmware entered"),
        (b"vof_getprop", b'"model"', b"[Pegasos2]"),
        (b"vof_write", b"SerenityOS PowerPC: model "),
        (b"vof_write", b'"Pegasos2"'),
    )
    missing = [fragments for fragments in expected if not any(
        all(fragment in line for fragment in fragments) for line in output.splitlines()
    )]
    serial_expected = (
        b"SerenityOS PowerPC: serial online",
        b"SerenityOS PowerPC: setjmp roundtrip passed",
        b"SerenityOS PowerPC: 32-bit kernel base 0xc0000000",
        f"SerenityOS PowerPC: RAM {args.memory} MiB".encode(),
    )
    serial_missing = [message for message in serial_expected if message not in serial_output]
    if missing or serial_missing:
        sys.stderr.write(output.decode("utf-8", errors="replace"))
        sys.stderr.write(serial_output.decode("utf-8", errors="replace"))
        sys.stderr.write(error_output.decode("utf-8", errors="replace"))
        sys.stderr.write(f"Missing expected firmware output: {missing!r}; missing serial output: {serial_missing!r}\n")
        return 1
    print("Pegasos 2 Open Firmware boot probe passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
