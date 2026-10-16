#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

import argparse
import os
from pathlib import Path


def check_dma_boot(log: str, require_desktop: bool, expected_mode: int = 2, require_stable_dma: bool = False) -> None:
    if "channel 0 in legacy mode" in log:
        raise AssertionError("the physical Pegasos 2 loses its IDE drive in legacy mode")
    if "StorageManagement: Detected 0 storage devices" in log or "KERNEL PANIC" in log:
        raise AssertionError("the IDE root device was unavailable")
    if f"IDE: Master drive using Ultra DMA mode {expected_mode}" not in log:
        raise AssertionError(f"the VIA drive did not start in UDMA{expected_mode}")
    if "IDE: Master drive DMA transfer failed" in log and "IDE: Channel reset after DMA failure, drive status" not in log:
        raise AssertionError("the drive was not reset before PIO fallback")
    if "IDEDevice: Request for block" in log or "Init process quit unexpectedly" in log:
        raise AssertionError("a disk request or the init process failed")
    if require_stable_dma and ("IDE: First DMA transfer succeeded" not in log or "IDE: Master drive DMA transfer failed" in log):
        raise AssertionError("DMA did not remain operational through boot")
    if require_stable_dma and "IDE: First IRQ-waited DMA transfer succeeded" not in log:
        raise AssertionError("DMA did not complete while waiting for its IRQ")
    if "SystemServer(9): Activating" not in log:
        raise AssertionError("the initial services did not start after disk access")
    if require_desktop and "Entering WindowServer main loop" not in log:
        raise AssertionError("the desktop did not start after disk access")


def check_default_pio_boot(log: str, require_desktop: bool) -> None:
    command_line = next((line.split("Kernel Commandline:", 1)[1].split() for line in log.splitlines()
                         if "Kernel Commandline:" in line), [])
    if not command_line or "dma" in command_line or "nodma" in command_line or any(
            argument.startswith("udma=") for argument in command_line):
        raise AssertionError("the loader must pass the unmodified PIO-default options")
    if "IDE: DMA requires an explicit dma or udma=N boot argument on this board" not in log:
        raise AssertionError("the Pegasos IDE controller did not select its PIO default")
    if "IDE: Master drive using Ultra DMA mode" in log or "KERNEL PANIC" in log:
        raise AssertionError("the default boot attempted DMA or crashed")
    if "Ext2FS: Mount successful" not in log:
        raise AssertionError("the PIO default did not mount the root filesystem")
    if require_desktop and "Entering WindowServer main loop" not in log:
        raise AssertionError("the PIO default did not reach the desktop")


def self_test() -> None:
    default_pio = """Kernel Commandline: fullram weakrng serial_debug root=lun3:0:0;part1
IDE: DMA requires an explicit dma or udma=N boot argument on this board
Ext2FS: Mount successful, setting superblock to error state.
Entering WindowServer main loop
"""
    check_default_pio_boot(default_pio, True)
    try:
        check_default_pio_boot(default_pio.replace("weakrng serial_debug", "weakrng nodma serial_debug"), True)
    except AssertionError as error:
        if "unmodified PIO-default options" not in str(error):
            raise
    else:
        raise AssertionError("the loader must not append the redundant nodma flag")
    try:
        check_default_pio_boot(default_pio.replace("weakrng serial_debug", "weakrng dma serial_debug"), True)
    except AssertionError as error:
        if "unmodified PIO-default options" not in str(error):
            raise
    else:
        raise AssertionError("the default PIO check must reject an explicit DMA boot")

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
    stalled_after_progress = recovered.replace(
        "IDE: DMA read of 8 sectors at 68136 failed, drive status 0x58, DMA status 0x21",
        "IDE: First DMA transfer succeeded: drive status 0x50, DMA status 0x24, handled interrupt true\n"
        "IDE: DMA read of 8 sectors at 635776 failed, drive status 0x58, drive error 0x00, DMA status 0x21\n"
        "IDE: DMA IRQ this transfer: 0 (recorded false, stopped engine false), BM at IRQ 0x00, ATA at IRQ 0x00\n"
        "IDE: DMA history: 134 successful transfers, 138 handled interrupts",
    )
    check_dma_boot(stalled_after_progress, True)
    try:
        check_dma_boot(stalled_after_progress, True, require_stable_dma=True)
    except AssertionError as error:
        if "DMA did not remain operational" not in str(error):
            raise
    else:
        raise AssertionError("a later DMA stall must fail the stable boot check")
    try:
        check_dma_boot(recovered, True, require_stable_dma=True)
    except AssertionError as error:
        if "DMA did not remain operational" not in str(error):
            raise
    else:
        raise AssertionError("PIO fallback must not count as stable DMA")
    stable = "IDE: First DMA transfer succeeded: drive status 0x50, DMA status 0x24, handled interrupt true\n" + recovered
    stable = stable.replace("IDE: DMA read of 8 sectors at 68136 failed, drive status 0x58, DMA status 0x21\n", "")
    stable = stable.replace("IDE: Master drive DMA transfer failed (Error(errno=69)), using PIO from now on\n", "")
    stable = stable.replace("IDE: Channel reset after DMA failure, drive status 0x50\n", "")
    try:
        check_dma_boot(stable, True, require_stable_dma=True)
    except AssertionError as error:
        if "waiting for its IRQ" not in str(error):
            raise
    else:
        raise AssertionError("a polled DMA boot must not pass the IRQ wait check")
    stable += "IDE: First IRQ-waited DMA transfer succeeded\n"
    check_dma_boot(stable, True, require_stable_dma=True)
    try:
        check_dma_boot("IDE: channel 0 in legacy mode\n" + recovered, True)
    except AssertionError as error:
        if "legacy mode" not in str(error):
            raise
    else:
        raise AssertionError("the observed physical legacy-mode boot failure must be rejected")
    check_dma_boot(recovered.replace("mode 2", "mode 0"), True, 0)
    retried = recovered.replace("IDE: Channel reset after DMA failure, drive status 0x50",
        "IDE: Channel reset after DMA failure attempt 1 timed out, drive status 0x58\n"
        "IDE: Channel reset after DMA failure, drive status 0x50, attempt 2")
    check_dma_boot(retried, True)
    stuck = recovered.replace("IDE: Channel reset after DMA failure, drive status 0x50\n",
        "IDE: Channel reset after DMA failure attempt 1 timed out, drive status 0x58\n"
        "IDE: Channel reset after DMA failure attempt 2 timed out, drive status 0x58\n"
        "IDEDevice: Request for block 151776 failed: Error(errno=69)\n")
    try:
        check_dma_boot(stuck, True)
    except AssertionError:
        pass
    else:
        raise AssertionError("a wedged drive must not count as a successful PIO fallback")
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
    parser.add_argument("--mode", type=int, default=2)
    parser.add_argument("--default-pio", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    path = os.environ.get("PEGASOS2_NATIVE_SERIAL_LOG")
    if not path:
        parser.error("set PEGASOS2_NATIVE_SERIAL_LOG to the physical serial capture")
    log = Path(path).read_text(errors="replace")
    if args.default_pio:
        check_default_pio_boot(log, args.normal)
    else:
        check_dma_boot(log, args.normal, args.mode, args.normal)


if __name__ == "__main__":
    main()
