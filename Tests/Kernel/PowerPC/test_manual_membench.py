#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

"""Check that the Pegasos root includes membench without starting it at login."""

import argparse
from pathlib import Path
import shutil
import subprocess


def debugfs_path():
    return shutil.which("debugfs") or "/opt/homebrew/opt/e2fsprogs/sbin/debugfs"


def inspect(image, command):
    result = subprocess.run([debugfs_path(), "-R", command, str(image)], text=True, capture_output=True, check=True)
    return result.stdout + result.stderr


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("--normal", action="store_true")
    args = parser.parse_args()
    if not args.image.is_file():
        parser.error(f"Root image does not exist: {args.image}")
    configuration = inspect(args.image, "cat /etc/SystemServer.ini")
    if "File not found" in configuration or "[BootBenchmark]" in configuration:
        raise SystemExit("membench must not start as a boot service")
    benchmark = inspect(args.image, "stat /bin/membench")
    if "File not found" in benchmark or "Type: regular" not in benchmark or "Mode:  0755" not in benchmark:
        raise SystemExit("/bin/membench must be installed and executable")
    if args.normal:
        boot_script = inspect(args.image, "stat /root/boot-benchmark.sh")
        if "File not found" not in boot_script:
            raise SystemExit("the automatic benchmark script is still installed")
    print("membench is installed for manual use and has no boot service")


if __name__ == "__main__":
    main()
