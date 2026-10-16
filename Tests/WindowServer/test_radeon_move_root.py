#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

"""Check the physical card carries the opt-in Radeon move path at 1024x768."""

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


def inspect(image, command):
    executable = shutil.which("debugfs") or "/opt/homebrew/opt/e2fsprogs/sbin/debugfs"
    result = subprocess.run([executable, "-R", command, str(image)], text=True, capture_output=True, check=True)
    return result.stdout + result.stderr


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    args = parser.parse_args()
    if not args.image.is_file():
        parser.error(f"Root image does not exist: {args.image}")
    config = inspect(args.image, "cat /etc/WindowServer.ini")
    if "Width=1024" not in config or "Height=768" not in config:
        raise SystemExit("the Pegasos diagnostic image changed the display resolution")
    with tempfile.TemporaryDirectory(prefix="radeon-move-root-") as directory:
        binary = Path(directory) / "WindowServer"
        output = inspect(args.image, f"dump /bin/WindowServer {binary}")
        if "File not found" in output or not binary.is_file():
            raise SystemExit("WindowServer is absent from the card root")
        contents = binary.read_bytes()
        if b"experimental Radeon 2D window moves enabled" not in contents:
            raise SystemExit("the installed WindowServer lacks the opt-in move path")
        if b"Radeon 2D copy self-test passed" not in contents:
            raise SystemExit("the installed WindowServer lacks the hardware self-test")
    print("Pegasos root contains the opt-in Radeon move path at 1024x768")


if __name__ == "__main__":
    main()
