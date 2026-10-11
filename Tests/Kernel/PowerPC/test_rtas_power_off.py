#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", required=True)
    parser.add_argument("--kernel", required=True)
    parser.add_argument("--memory", type=int, default=512)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="serenity-pegasos2-rtas-") as directory:
        serial_path = Path(directory) / "serial.log"
        command = [
            args.qemu,
            "-M", "pegasos2",
            "-m", str(args.memory),
            "-kernel", args.kernel,
            "-append", "serenity-rtas-power-off-test",
            "-display", "none",
            "-serial", f"file:{serial_path}",
            "-monitor", "none",
            "-no-reboot",
        ]
        try:
            result = subprocess.run(command, capture_output=True, timeout=15)
        except subprocess.TimeoutExpired as error:
            serial = serial_path.read_bytes() if serial_path.exists() else b""
            raise SystemExit(
                f"Pegasos 2 RTAS power-off timed out: {error}\n"
                + serial.decode("utf-8", errors="replace")
            ) from error
        serial = serial_path.read_bytes() if serial_path.exists() else b""
        if result.returncode != 0 or b"SerenityOS PowerPC: RTAS power-off requested" not in serial:
            raise SystemExit(
                f"Pegasos 2 RTAS power-off failed (exit {result.returncode}):\n"
                + serial.decode("utf-8", errors="replace")
                + result.stderr.decode("utf-8", errors="replace")
            )
    print("Pegasos 2 RTAS power-off passed")


if __name__ == "__main__":
    main()
