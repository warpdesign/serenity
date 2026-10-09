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

    serial_expected = (
        b"SerenityOS PowerPC: serial online",
        b"SerenityOS PowerPC: setjmp roundtrip passed",
        b"SerenityOS PowerPC: CPU features passed",
        b"SerenityOS PowerPC: processor primitives passed",
        b"SerenityOS PowerPC: memory fences passed",
        b"SerenityOS PowerPC: cache maintenance passed",
        b"SerenityOS PowerPC: big-endian short string passed",
        b"SerenityOS PowerPC: big-endian float digits passed",
        b"SerenityOS PowerPC: big-endian float parser passed",
        b"SerenityOS PowerPC: FPSCR environment passed",
        b"SerenityOS PowerPC: FPU state roundtrip passed",
        b"SerenityOS PowerPC: ELF32 big-endian header passed",
        b"SerenityOS PowerPC: ELF32 big-endian image passed",
        b"SerenityOS PowerPC: ELF32 relative relocations passed",
        b"SerenityOS PowerPC: PPC32 TLS layout passed",
        b"SerenityOS PowerPC: PPC32 static TLS lookup passed",
        b"SerenityOS PowerPC: ELF32 entry transfer passed",
        b"SerenityOS PowerPC: secure PLT lazy binding passed",
        b"SerenityOS PowerPC: 32-bit kernel base 0xc0000000",
        f"SerenityOS PowerPC: RAM {args.memory} MiB".encode(),
        b"SerenityOS PowerPC: program exception roundtrip passed",
        b"SerenityOS PowerPC: loader breakpoint passed",
        b"SerenityOS PowerPC: syscall register ABI passed",
        b"SerenityOS PowerPC: MMU high alias passed",
        b"SerenityOS PowerPC: HPT high alias passed",
        b"SerenityOS PowerPC: HPT duplicate mapping rejected",
        b"SerenityOS PowerPC: HPT remap and secondary hash passed",
        b"SerenityOS PowerPC: HPT group reuse passed",
        b"SerenityOS PowerPC: HPT unmapped fault passed",
        b"SerenityOS PowerPC: HPT read-only fault passed",
        b"SerenityOS PowerPC: HPT VSID switch passed",
        b"SerenityOS PowerPC: HPT kernel protection passed",
        b"SerenityOS PowerPC: user-mode page protection passed",
        b"SerenityOS PowerPC: user-mode stack passed",
        b"SerenityOS PowerPC: user-mode syscall resume passed",
        b"SerenityOS PowerPC: user-mode exception frame passed",
        b"SerenityOS PowerPC: register state from user trap passed",
        b"SerenityOS PowerPC: stack backchain unwinder passed",
    )

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
                if b'vof_write' in output and b'"Pegasos2"' in output and all(message in serial_output for message in serial_expected):
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
