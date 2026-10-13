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

SENDKEY_NAMES = {" ": "spc", ">": "shift-dot", "/": "slash", ".": "dot", "-": "minus"}
COMMAND = "echo ps2-keyboard-ok > /home/anon/ps2-input.txt"


def boot_and_type(args, disk, work):
    serial = work / "serial.log"
    before = work / "before-mouse.ppm"
    after = work / "after-mouse.ppm"
    with tempfile.TemporaryDirectory(prefix="powerpc-ps2-monitor-", dir="/tmp") as monitor_directory:
        monitor_path = Path(monitor_directory) / "monitor.sock"
        # No VirtIO input devices: QEMU routes host input to the VT8231's i8042 PS/2 ports.
        command = [
            args.qemu, "-M", "pegasos2", "-cpu", args.cpu, "-m", str(args.memory),
            "-vga", "none", "-device", "virtio-gpu-pci",
            "-kernel", str(args.loader), "-append", "root=lun3:0:0",
            "-drive", f"if=none,format=raw,id=virtio_disk,file={disk}",
            "-device", "virtio-blk-pci,drive=virtio_disk",
            "-object", "rng-random,id=rng0,filename=/dev/urandom", "-device", "virtio-rng-pci,rng=rng0",
            "-display", "none", "-serial", f"file:{serial}",
            "-monitor", f"unix:{monitor_path},server=on,wait=off", "-no-reboot",
        ]
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        mouse_moved = False
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
                        time.sleep(3)
                        test_window_server.monitor_command(monitor, f"screendump {before}")
                        test_window_server.monitor_command(monitor, "mouse_move 120 90")
                        time.sleep(2)
                        test_window_server.monitor_command(monitor, f"screendump {after}")
                        mouse_moved = before.read_bytes() != after.read_bytes()
                        for character in COMMAND:
                            test_window_server.monitor_command(monitor, f"sendkey {SENDKEY_NAMES.get(character, character)}")
                            time.sleep(0.15)
                        test_window_server.monitor_command(monitor, "sendkey ret")
                        # Give the guest's periodic filesystem sync time to write the file.
                        time.sleep(15)
                        break
                    time.sleep(0.5)
        finally:
            process.terminate()
            _, errors = process.communicate(timeout=5)

    output = serial.read_bytes() if serial.exists() else b""
    typed = subprocess.run(
        [test_desktop.find_debugfs(), "-R", "cat /home/anon/ps2-input.txt", str(disk)],
        capture_output=True, text=True,
    ).stdout.strip()
    if (b"I8042: Dual channel controller" not in output or not mouse_moved or typed != "ps2-keyboard-ok"
            or b"KERNEL PANIC" in output or b"CRASH:" in output):
        raise SystemExit(
            f"Pegasos 2 PS/2 input failed: mouse moved={mouse_moved}, typed={typed!r}\n"
            + output.decode(errors="replace")[-3500:] + errors.decode(errors="replace")[-1000:]
        )
    print("Pegasos 2 i8042 PS/2 mouse moved the cursor and keyboard typed into Terminal")


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
    with tempfile.TemporaryDirectory(prefix="powerpc-ps2-input-", dir="/tmp") as directory:
        work = Path(directory)
        root = test_desktop.create_root(source, args.build_dir, args.user_loader, work)
        disk = test_desktop.create_disk(root, work)
        boot_and_type(args, disk, work)


if __name__ == "__main__":
    main()
