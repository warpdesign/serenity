#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

"""Check the physical card root enables the compositor timing build."""

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


def debugfs(image, command):
    executable = shutil.which("debugfs") or "/opt/homebrew/opt/e2fsprogs/sbin/debugfs"
    result = subprocess.run([executable, "-R", command, str(image)], text=True, capture_output=True, check=True)
    return result.stdout + result.stderr


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    args = parser.parse_args()
    if not args.image.is_file():
        parser.error(f"Root image does not exist: {args.image}")

    configuration = debugfs(args.image, "cat /etc/WindowServer.ini")
    if "[Debug]\nProfileComposition=true" not in configuration:
        raise SystemExit("WindowServer composition profiling is not enabled")

    with tempfile.TemporaryDirectory(prefix="windowserver-profile-") as directory:
        binary = Path(directory) / "WindowServer"
        output = debugfs(args.image, f"dump /bin/WindowServer {binary}")
        if "File not found" in output or not binary.is_file():
            raise SystemExit("WindowServer is absent from the card root")
        if b"WindowServer perf: frames" not in binary.read_bytes():
            raise SystemExit("the installed WindowServer lacks composition timing")
    print("Pegasos root enables the instrumented WindowServer")


if __name__ == "__main__":
    main()
