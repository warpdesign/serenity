#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

import argparse
import os
import re
from pathlib import Path


def check_boot(log: str, require_userland: bool, require_rtas: bool = False,
        require_uhci_progress: bool = False, require_entropy_progress: bool = False,
        require_ps2_mouse: bool = False) -> None:
    if require_rtas and "PCI: Pegasos 2 using RTAS PCI configuration" not in log:
        raise AssertionError("RTAS PCI configuration did not succeed")
    if ("PCI: Pegasos 2 using RTAS PCI configuration" not in log
            and "PCI: Pegasos 2 direct fallback south 1106:8231, IDE 1106:0571" not in log):
        raise AssertionError("neither PCI configuration path identified the VT8231 and VIA IDE")
    if "PCI: Pegasos 2 native IDE 1106:0571" not in log:
        raise AssertionError("PCI enumeration did not identify the VIA IDE controller")
    if require_uhci_progress:
        if "UHCI: PCI [0000:00:0c:02]: Interrupt line: 9" not in log:
            raise AssertionError("the first UHCI controller did not start probing")
        if not any(marker in log for marker in ("UHCI: Reset completed", "UHCI: Started",
                "Controller stop timed out", "Controller reset timed out", "Controller start timed out")):
            raise AssertionError("UHCI probing stopped without completing or reporting a timeout")
    detected = re.search(r"StorageManagement: Detected (\d+) storage devices", log)
    if detected is None or int(detected.group(1)) < 1:
        raise AssertionError("the IDE controller did not provide a storage device")
    if "KERNEL PANIC!" in log:
        raise AssertionError("the kernel panicked")
    if require_entropy_progress:
        if "PowerPC: Insecure interrupt-timing entropy pool ready" not in log:
            raise AssertionError("the opt-in PowerPC entropy pool did not become ready")
        if require_userland and "Entering WindowServer main loop" not in log:
            raise AssertionError("WindowServer did not start after entropy became ready")
    if require_ps2_mouse:
        if "I8042: Continuing with second port after keyboard probe failure" not in log:
            raise AssertionError("i8042 did not continue past the absent keyboard")
        if "PS2MouseDevice: Mouse wheel enabled!" not in log and "PS2MouseDevice: No mouse wheel detected!" not in log:
            raise AssertionError("the PS/2 mouse did not finish initialization")
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
    check_boot(recovered, True, True)
    fallback = recovered.replace("PCI: Pegasos 2 using RTAS PCI configuration",
        "PCI: Pegasos 2 direct fallback south 1106:8231, IDE 1106:0571")
    check_boot(fallback, True)
    previous_uhci_hang = fallback.split("StorageManagement: Detected", 1)[0]
    previous_uhci_hang += "UHCI: PCI [0000:00:0c:02]: Interrupt line: 9\n"
    try:
        check_boot(previous_uhci_hang, False)
    except AssertionError:
        pass
    else:
        raise AssertionError("a boot stuck during UHCI initialization must be rejected")
    boot_after_usb_timeout = fallback.replace("StorageManagement: Detected",
        "UHCI: PCI [0000:00:0c:02]: Interrupt line: 9\n"
        "UHCI: Controller reset timed out, USBCMD=0xffff, USBSTS=0xffff\nStorageManagement: Detected")
    check_boot(boot_after_usb_timeout, True, require_uhci_progress=True)
    entropy_ready_boot = recovered.replace("Running first user process:",
        "PowerPC: Insecure interrupt-timing entropy pool ready\nRunning first user process:")
    entropy_ready_boot += "Entering WindowServer main loop\n"
    check_boot(entropy_ready_boot, True, require_entropy_progress=True)
    mouse_boot = entropy_ready_boot.replace("Running first user process:",
        "I8042: Continuing with second port after keyboard probe failure\n"
        "PS2MouseDevice: No mouse wheel detected!\nRunning first user process:")
    check_boot(mouse_boot, True, require_ps2_mouse=True)
    try:
        check_boot(entropy_ready_boot, True, require_ps2_mouse=True)
    except AssertionError:
        pass
    else:
        raise AssertionError("a skipped PS/2 mouse probe must fail")
    try:
        check_boot(recovered + "Entropy starvation...\n", True, require_entropy_progress=True)
    except AssertionError:
        pass
    else:
        raise AssertionError("a boot without entropy readiness must fail")
    try:
        check_boot(previous_uhci_hang, False, require_uhci_progress=True)
    except AssertionError:
        pass
    else:
        raise AssertionError("the earlier UHCI hang must fail the UHCI check")
    try:
        check_boot(fallback, True, True)
    except AssertionError:
        pass
    else:
        raise AssertionError("direct fallback must not pass the RTAS-specific check")


def main() -> None:
    parser = argparse.ArgumentParser(description="Check a physical Pegasos 2 serial boot log")
    parser.add_argument("--normal", action="store_true", help="also require userland startup")
    parser.add_argument("--require-rtas", action="store_true", help="require PCI access through RTAS")
    parser.add_argument("--require-uhci-progress", action="store_true", help="require UHCI to finish or time out")
    parser.add_argument("--require-entropy-progress", action="store_true", help="require opt-in PowerPC entropy readiness")
    parser.add_argument("--require-ps2-mouse", action="store_true", help="require PS/2 mouse startup after a missing keyboard")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    path = os.environ.get("PEGASOS2_NATIVE_SERIAL_LOG")
    if not path:
        parser.error("set PEGASOS2_NATIVE_SERIAL_LOG to the physical serial capture")
    check_boot(Path(path).read_text(errors="replace"), args.normal, args.require_rtas,
        args.require_uhci_progress, args.require_entropy_progress, args.require_ps2_mouse)


if __name__ == "__main__":
    main()
