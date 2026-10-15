#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

import argparse
import os
from pathlib import Path


def check_keyboard_enumeration(log: str, require_initialized: bool) -> None:
    if "USB Hub: Device attached to port 2!" not in log:
        raise AssertionError("UHCI did not detect the keyboard on port 2")
    if "USB Device returned incorrect descriptor type" in log:
        raise AssertionError("the keyboard device descriptor was not received")
    if "USB Hub: Failed to create device for port 2" in log:
        raise AssertionError("the keyboard failed USB enumeration")
    if "USB HID: Found HID interface" not in log:
        raise AssertionError("USB HID did not find a keyboard interface")
    if require_initialized and "USB HID: Initialized interface" not in log:
        raise AssertionError("USB HID did not finish initializing the keyboard")


def self_test() -> None:
    previous_failure = """USB Hub: Device attached to port 2!
USB Device returned incorrect descriptor type for GetDescriptor(Device) request - Expected 0x1 but got 0x0
USB Hub: Failed to create device for port 2: Error(errno=5)
"""
    try:
        check_keyboard_enumeration(previous_failure, False)
    except AssertionError:
        pass
    else:
        raise AssertionError("the observed keyboard failure must be rejected")

    recovered = """USB Hub: Device attached to port 2!
USB HID: Found HID interface of device 1234:5678 at interface ID 0
USB HID: Initialized interface 0 of device 1234:5678
"""
    check_keyboard_enumeration(recovered, False)
    check_keyboard_enumeration(recovered, True)
    try:
        check_keyboard_enumeration(recovered.replace("USB HID: Initialized interface 0 of device 1234:5678\n", ""), True)
    except AssertionError:
        pass
    else:
        raise AssertionError("the normal check must require initialized HID input")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--normal", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    log_path = os.environ.get("PEGASOS2_NATIVE_SERIAL_LOG")
    if not log_path:
        parser.error("set PEGASOS2_NATIVE_SERIAL_LOG to the physical serial capture")
    check_keyboard_enumeration(Path(log_path).read_text(errors="replace"), args.normal)


if __name__ == "__main__":
    main()
