#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

import argparse
import os
import re
from pathlib import Path


def check_boot(log: str, require_userland: bool) -> None:
    if ("PCI: Pegasos 2 using RTAS PCI configuration" not in log
            and "PCI: Pegasos 2 direct fallback south 1106:8231, IDE 1106:0571" not in log):
        raise AssertionError("neither PCI configuration path identified the VT8231 and VIA IDE")
    if "PCI: Pegasos 2 native IDE 1106:0571" not in log:
        raise AssertionError("PCI enumeration did not identify the VIA IDE controller")
    detected = re.search(r"StorageManagement: Detected (\d+) storage devices", log)
    if detected is None or int(detected.group(1)) < 1:
        raise AssertionError("the IDE controller did not provide a storage device")
    if "KERNEL PANIC!" in log:
        raise AssertionError("the kernel panicked")
    if require_userland and "Running first user process:" not in log:
        raise AssertionError("the kernel did not start userland")


def self_test() -> None:
    previous_failure = """PCI: Failed to assign Pegasos 2 device resources
StorageManagement: Detected 0 storage devices
KERNEL PANIC! :^(
"""
    try:
        check_boot(previous_failure, False)
    except AssertionError:
        pass
    else:
        raise AssertionError("the previous physical failure must be rejected")
    recovered = """PCI: Pegasos 2 using RTAS PCI configuration
PCI: Pegasos 2 native IDE 1106:0571, display 1002:5964; preserving firmware BARs
StorageManagement: Detected 1 storage devices
Running first user process: /sbin/init
"""
    check_boot(recovered, False)
    check_boot(recovered, True)
    fallback = recovered.replace("PCI: Pegasos 2 using RTAS PCI configuration",
        "PCI: Pegasos 2 direct fallback south 1106:8231, IDE 1106:0571")
    check_boot(fallback, True)


def main() -> None:
    parser = argparse.ArgumentParser(description="Check a physical Pegasos 2 serial boot log")
    parser.add_argument("--normal", action="store_true", help="also require userland startup")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    path = os.environ.get("PEGASOS2_NATIVE_SERIAL_LOG")
    if not path:
        parser.error("set PEGASOS2_NATIVE_SERIAL_LOG to the physical serial capture")
    check_boot(Path(path).read_text(errors="replace"), args.normal)


if __name__ == "__main__":
    main()
