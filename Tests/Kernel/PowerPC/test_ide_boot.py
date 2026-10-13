#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

import argparse
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import tempfile
import time

sys.dont_write_bytecode = True
import test_desktop
import test_window_server

SENDKEY_NAMES = {" ": "spc", ">": "shift-dot", "/": "slash", ".": "dot", "-": "minus"}
LINE_COUNT = 20000
COMMAND = f"seq 1 {LINE_COUNT} > /home/anon/ide-write.txt"


def find_e2fsck():
    e2fsck = shutil.which("e2fsck")
    if e2fsck:
        return e2fsck
    homebrew_e2fsck = Path("/opt/homebrew/opt/e2fsprogs/sbin/e2fsck")
    if homebrew_e2fsck.exists():
        return str(homebrew_e2fsck)
    raise SystemExit("e2fsck is required for the PowerPC IDE boot test")


def boot_and_write(args, disk, work):
    serial = work / "serial.log"
    with tempfile.TemporaryDirectory(prefix="powerpc-ide-monitor-", dir="/tmp") as monitor_directory:
        monitor_path = Path(monitor_directory) / "monitor.sock"
        # The root filesystem is the only disk, attached to the VT8231 IDE primary master.
        command = [
            args.qemu, "-M", "pegasos2", "-cpu", args.cpu, "-m", str(args.memory),
            "-vga", "none", "-device", "virtio-gpu-pci", "-device", "virtio-keyboard-pci",
            "-device", "virtio-mouse-pci", "-kernel", str(args.loader), "-append", "root=lun3:0:0",
            "-drive", f"if=ide,format=raw,index=0,file={disk}",
            "-object", "rng-random,id=rng0,filename=/dev/urandom", "-device", "virtio-rng-pci,rng=rng0",
            "-display", "none", "-serial", f"file:{serial}",
            "-monitor", f"unix:{monitor_path},server=on,wait=off", "-no-reboot",
        ]
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        typed = False
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
                        for character in COMMAND:
                            test_window_server.monitor_command(monitor, f"sendkey {SENDKEY_NAMES.get(character, character)}")
                            time.sleep(0.12)
                        test_window_server.monitor_command(monitor, "sendkey ret")
                        typed = True
                        # Give the guest's periodic filesystem sync time to write the file.
                        time.sleep(15)
                        break
                    time.sleep(0.5)
        finally:
            process.terminate()
            _, errors = process.communicate(timeout=5)

    output = serial.read_bytes() if serial.exists() else b""
    written = work / "ide-write.txt"
    subprocess.run([test_desktop.find_debugfs(), "-R", f"dump /home/anon/ide-write.txt {written}", str(disk)],
                   capture_output=True)
    expected = "".join(f"{n}\n" for n in range(1, LINE_COUNT + 1)).encode()
    file_matches = written.exists() and written.read_bytes() == expected
    fsck = subprocess.run([find_e2fsck(), "-fn", str(disk)], capture_output=True, text=True)
    if (b'IDE: Master drive "QEMU HARDDISK"' not in output or not typed or not file_matches or fsck.returncode != 0
            or b"KERNEL PANIC" in output or b"CRASH:" in output):
        raise SystemExit(
            f"Pegasos 2 IDE boot failed: typed={typed}, file matches={file_matches}, e2fsck={fsck.returncode}\n"
            + fsck.stdout[-1000:] + output.decode(errors="replace")[-3500:] + errors.decode(errors="replace")[-1000:]
        )
    print(f"Pegasos 2 booted the desktop from VT8231 IDE and wrote {len(expected)} bytes with a clean filesystem")


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
    with tempfile.TemporaryDirectory(prefix="powerpc-ide-boot-", dir="/tmp") as directory:
        work = Path(directory)
        root = test_desktop.create_root(source, args.build_dir, args.user_loader, work)
        disk = test_desktop.create_disk(root, work)
        boot_and_write(args, disk, work)


if __name__ == "__main__":
    main()
