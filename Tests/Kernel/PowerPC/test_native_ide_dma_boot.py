#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

import argparse
import os
from pathlib import Path


def check_dma_boot(log: str, require_desktop: bool) -> None:
    if "IDE: Master drive using Ultra DMA mode 2" not in log:
        raise AssertionError("the unconfigured VIA drive did not start in conservative UDMA2")
    if "IDE: Master drive DMA transfer failed" in log and "IDE: Channel reset after DMA failure, drive status" not in log:
        raise AssertionError("the drive was not reset before PIO fallback")
    if "IDEDevice: Request for block" in log or "Init process quit unexpectedly" in log:
        raise AssertionError("a disk request or the init process failed")
    if "SystemServer(9): Activating" not in log:
        raise AssertionError("the initial services did not start after disk access")
    if require_desktop and "Entering WindowServer main loop" not in log:
        raise AssertionError("the desktop did not start after disk access")


def self_test() -> None:
    failure = """IDE: Master drive using Ultra DMA mode 5
IDE: DMA read of 8 sectors at 68136 failed, drive status 0x58, DMA status 0x21
IDE: Master drive DMA transfer failed (Error(errno=69)), using PIO from now on
IDEDevice: Request for block 68136 failed: Error(errno=69)
Init process quit unexpectedly. Exit code: 1
"""
    try:
        check_dma_boot(failure, False)
    except AssertionError:
        pass
    else:
        raise AssertionError("the observed physical DMA boot failure must be rejected")

    recovered = """IDE: Master drive using Ultra DMA mode 2
IDE: DMA read of 8 sectors at 68136 failed, drive status 0x58, DMA status 0x21
IDE: Master drive DMA transfer failed (Error(errno=69)), using PIO from now on
IDE: Channel reset after DMA failure, drive status 0x50
SystemServer(9): Activating 4 services...
Entering WindowServer main loop
"""
    check_dma_boot(recovered, False)
    check_dma_boot(recovered, True)
    try:
        check_dma_boot(recovered.replace("IDE: Channel reset after DMA failure, drive status 0x50\n", ""), True)
    except AssertionError:
        pass
    else:
        raise AssertionError("a fallback without channel reset must fail")
    check_dma_boot(recovered.replace("IDE: DMA read of 8 sectors at 68136 failed, drive status 0x58, DMA status 0x21\n"
        "IDE: Master drive DMA transfer failed (Error(errno=69)), using PIO from now on\n"
        "IDE: Channel reset after DMA failure, drive status 0x50\n", ""), True)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--normal", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    path = os.environ.get("PEGASOS2_NATIVE_SERIAL_LOG")
    if not path:
        parser.error("set PEGASOS2_NATIVE_SERIAL_LOG to the physical serial capture")
    check_dma_boot(Path(path).read_text(errors="replace"), args.normal)


if __name__ == "__main__":
    main()
