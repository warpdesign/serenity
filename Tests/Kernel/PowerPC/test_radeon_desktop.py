#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

import argparse
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time

sys.dont_write_bytecode = True
import test_desktop
import test_window_server


def boot_and_check(args, disk, work):
    serial = work / "serial.log"
    screenshot = work / "desktop.ppm"
    with tempfile.TemporaryDirectory(prefix="powerpc-radeon-monitor-", dir="/tmp") as monitor_directory:
        monitor_path = Path(monitor_directory) / "monitor.sock"
        # QEMU's Radeon 7000 (RV100) model is the only display device.
        command = [
            args.qemu, "-M", "pegasos2", "-cpu", args.cpu, "-m", str(args.memory),
            "-vga", "none", "-device", "ati-vga,model=rv100",
            "-device", "virtio-keyboard-pci", "-device", "virtio-mouse-pci",
            "-kernel", str(args.loader), "-append", "root=lun3:0:0",
            "-drive", f"if=none,format=raw,id=virtio_disk,file={disk}",
            "-device", "virtio-blk-pci,drive=virtio_disk",
            "-object", "rng-random,id=rng0,filename=/dev/urandom", "-device", "virtio-rng-pci,rng=rng0",
            "-display", "none", "-serial", f"file:{serial}",
            "-monitor", f"unix:{monitor_path},server=on,wait=off", "-no-reboot",
        ]
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        passed = False
        try:
            deadline = time.monotonic() + args.timeout
            while not monitor_path.exists() and time.monotonic() < deadline and process.poll() is None:
                time.sleep(0.05)
            with socket.socket(socket.AF_UNIX) as monitor:
                monitor.settimeout(5)
                monitor.connect(str(monitor_path))
                greeting = b""
                while not greeting.endswith(b"(qemu) "):
                    greeting += monitor.recv(16384)
                while time.monotonic() < deadline and process.poll() is None:
                    output = serial.read_bytes() if serial.exists() else b""
                    if b"KERNEL PANIC" in output or b"CRASH:" in output:
                        break
                    if b"Entering WindowServer main loop" in output and b"Shell(" in output:
                        test_window_server.monitor_command(monitor, f"screendump {screenshot}")
                        # The desktop check samples exact colours, so it also catches pixel byte-order mistakes.
                        if test_desktop.screenshot_has_desktop(screenshot):
                            passed = True
                            break
                    time.sleep(1)
        finally:
            process.terminate()
            _, errors = process.communicate(timeout=5)

    output = serial.read_bytes() if serial.exists() else b""
    if not passed or b"Radeon: 1024x768 with pixel clock 65000 kHz" not in output or b"KERNEL PANIC" in output:
        raise SystemExit("Pegasos 2 Radeon desktop failed:\n" + output.decode(errors="replace")[-4000:]
                         + errors.decode(errors="replace")[-1000:])
    print(f"Pegasos 2 Radeon RV100 showed the desktop with correct colours: {screenshot}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", required=True)
    parser.add_argument("--loader", type=Path, required=True)
    parser.add_argument("--user-loader", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--cpu", default="750cxe")
    parser.add_argument("--memory", type=int, default=512)
    parser.add_argument("--timeout", type=int, default=300)
    args = parser.parse_args()
    source = Path(__file__).parents[3]
    with tempfile.TemporaryDirectory(prefix="powerpc-radeon-desktop-", dir="/tmp") as directory:
        work = Path(directory)
        root = test_desktop.create_root(source, args.build_dir, args.user_loader, work)
        disk = test_desktop.create_disk(root, work)
        boot_and_check(args, disk, work)


if __name__ == "__main__":
    main()
