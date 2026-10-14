#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

"""Measure input-to-screen latency of the PowerPC desktop in headless QEMU.

Boots a PixelPaint disk image (the one test_pixel_paint.py --output-dir leaves behind, where
PixelPaint starts on login), then drives the mouse through the QEMU monitor and polls
`screendump` to time:
  - cursor-only moves, optionally after idle gaps (--gaps);
  - a single 25-pixel paint segment;
  - a burst of small moves at --interval, i.e. how long drawing lags behind the cursor.

With --plugin pointing at a build of qemu-tbprof.c, the burst is also profiled; the work directory
then contains profile.txt for symbolize-tbprof.py.
"""

import argparse
import shutil
import socket
import subprocess
import tempfile
import time
from pathlib import Path

WIDTH = 1024


class Monitor:
    def __init__(self, path):
        self.socket = socket.socket(socket.AF_UNIX)
        self.socket.settimeout(10)
        self.socket.connect(str(path))
        self.read_prompt()

    def read_prompt(self):
        data = b""
        while not data.endswith(b"(qemu) "):
            data += self.socket.recv(65536)
        return data

    def command(self, text):
        self.socket.sendall(text.encode() + b"\n")
        return self.read_prompt()


def pixel(image, x, y):
    offset = (y * WIDTH + x) * 3
    return image[offset:offset + 3]


def wait_until(predicate, timeout):
    start = time.monotonic()
    while time.monotonic() - start < timeout:
        if predicate():
            return time.monotonic() - start
    return None


def milliseconds(seconds):
    return "timeout" if seconds is None else f"{seconds * 1000:.0f} ms"


def main():
    source = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("disk", type=Path, help="PixelPaint root.ext2; a copy is booted")
    parser.add_argument("--build-dir", type=Path, default=source / "Build/powerpc")
    parser.add_argument("--qemu", default="qemu-system-ppc")
    parser.add_argument("--cpu", default="750cxe")
    parser.add_argument("--memory", default="512")
    parser.add_argument("--burst", type=int, default=30)
    parser.add_argument("--step", type=int, default=3, help="pixels per burst move")
    parser.add_argument("--interval", type=float, default=0.02, help="seconds between burst moves")
    parser.add_argument("--gaps", type=float, nargs="*", default=[], help="idle seconds before extra cursor moves")
    parser.add_argument("--plugin", type=Path, help="qemu-tbprof plugin to profile the burst")
    parser.add_argument("--extra", nargs="*", default=[], help="extra QEMU arguments")
    args = parser.parse_args()

    work = Path(tempfile.mkdtemp(prefix="serenity-latency-"))
    disk = work / "root.ext2"
    shutil.copy(args.disk, disk)
    serial = work / "serial.log"
    monitor_path = work / "monitor.sock"
    screenshot = work / "screen.ppm"
    control = work / "tbprof"
    loader = args.build_dir / "Kernel/Arch/powerpc/Loader/image/serenity-powerpc-loader.elf"
    command = [args.qemu, "-M", "pegasos2", "-cpu", args.cpu, "-m", args.memory,
               "-vga", "none", "-device", "virtio-gpu-pci", "-device", "virtio-keyboard-pci",
               "-device", "virtio-mouse-pci", "-kernel", str(loader), "-append", "root=lun3:0:0",
               "-drive", f"if=none,format=raw,id=disk,file={disk}", "-device", "virtio-blk-pci,drive=disk",
               "-object", "rng-random,id=rng0,filename=/dev/urandom", "-device", "virtio-rng-pci,rng=rng0",
               "-display", "none", "-serial", f"file:{serial}",
               "-monitor", f"unix:{monitor_path},server=on,wait=off", "-no-reboot", *args.extra]
    if args.plugin:
        control.mkdir()
        command += ["-plugin", f"{args.plugin},ctl={control}"]
    process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    try:
        while not monitor_path.exists():
            time.sleep(0.05)
        monitor = Monitor(monitor_path)

        def grab():
            monitor.command(f"screendump {screenshot}")
            return screenshot.read_bytes().split(b"\n", 3)[3]

        deadline = time.monotonic() + 300
        while time.monotonic() < deadline:
            output = serial.read_bytes() if serial.exists() else b""
            if b"Entering WindowServer main loop" in output and b"PixelPaint(" in output:
                break
            time.sleep(0.5)
        time.sleep(15)

        for gap in args.gaps:
            time.sleep(gap)
            before = grab()
            monitor.command("mouse_move 4 0")
            print(f"idle {gap:4.1f}s -> cursor visible after {milliseconds(wait_until(lambda: grab() != before, 10))}")
        for index in range(6):
            before = grab()
            monitor.command("mouse_move 60 0" if index % 2 == 0 else "mouse_move -60 0")
            print(f"cursor-only move {index} visible after {milliseconds(wait_until(lambda: grab() != before, 10))}")
            time.sleep(1)

        # The cursor starts at the screen centre; this puts it on PixelPaint's canvas at (400, 350).
        monitor.command("mouse_move -112 -34")
        time.sleep(2)
        monitor.command("mouse_button 1")
        time.sleep(2)
        before = grab()
        monitor.command("mouse_move 25 0")
        painted = wait_until(lambda: sum(pixel(grab(), x, 350) == b"\0\0\0" and pixel(before, x, 350) != b"\0\0\0"
                                         for x in range(402, 424)) > 10, 20)
        print(f"single 25px paint segment visible after {milliseconds(painted)}")
        time.sleep(2)

        if args.plugin:
            (control / "reset").touch()
            while (control / "reset").exists():
                time.sleep(0.01)
        start = time.monotonic()
        for _ in range(args.burst):
            monitor.command(f"mouse_move 0 {args.step}")
            time.sleep(args.interval)
        sent = time.monotonic() - start
        # Probe the line above the cursor, so the black crosshair is not mistaken for paint.
        probe_y = 350 + args.step * args.burst - 20
        caught_up = wait_until(lambda: any(pixel(grab(), x, probe_y) == b"\0\0\0" for x in range(422, 429)), 60)
        if args.plugin:
            (control / "dump").touch()
            while not (control / "done").exists():
                time.sleep(0.05)
            shutil.copy(control / "profile.txt", work / "profile.txt")
        print(f"burst of {args.burst} moves sent over {sent * 1000:.0f} ms; drawing caught up {milliseconds(caught_up)} after the last move")
        monitor.command("mouse_button 0")
        grab()
        shutil.copy(screenshot, work / "after-burst.ppm")
        print(f"work directory: {work}")
    finally:
        process.terminate()
        process.communicate(timeout=10)


if __name__ == "__main__":
    main()
