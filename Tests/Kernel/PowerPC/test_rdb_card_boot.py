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
COMMAND = f"seq 1 {LINE_COUNT} > /home/anon/card-write.txt"


# Matches Meta/PowerPC/make-pegasos-card.py: 1 MiB of RDB, then the 32 MiB boot partition.
ROOT_FIRST_SECTOR = 2048 + 65536


def extract_partition(card, first_sector, output):
    with card.open("rb") as source, output.open("wb") as destination:
        source.seek(first_sector * 512)
        while chunk := source.read(1 << 20):
            destination.write(chunk)


def find_e2fsck():
    e2fsck = shutil.which("e2fsck")
    if e2fsck:
        return e2fsck
    homebrew_e2fsck = Path("/opt/homebrew/opt/e2fsprogs/sbin/e2fsck")
    if homebrew_e2fsck.exists():
        return str(homebrew_e2fsck)
    raise SystemExit("e2fsck is required for the PowerPC IDE boot test")


def boot_and_write(args, card, work):
    serial = work / "serial.log"
    with tempfile.TemporaryDirectory(prefix="powerpc-card-monitor-", dir="/tmp") as monitor_directory:
        monitor_path = Path(monitor_directory) / "monitor.sock"
        # The SD card image is the only disk, on the VT8231 IDE primary master, as on the physical machine.
        command = [
            args.qemu, "-M", "pegasos2", "-cpu", args.cpu, "-m", str(args.memory),
            "-vga", "none", "-device", "virtio-gpu-pci", "-device", "virtio-keyboard-pci",
            "-device", "virtio-mouse-pci", "-kernel", str(args.loader), "-append",
            "native realmode" if args.nativeboot else "root=lun3:0:0;part1",
            "-drive", f"if=ide,format=raw,index=0,file={card}",
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
    disk = work / "root-after.ext2"
    extract_partition(card, ROOT_FIRST_SECTOR, disk)
    written = work / "card-write.txt"
    subprocess.run([test_desktop.find_debugfs(), "-R", f"dump /home/anon/card-write.txt {written}", str(disk)],
                   capture_output=True)
    expected = "".join(f"{n}\n" for n in range(1, LINE_COUNT + 1)).encode()
    file_matches = written.exists() and written.read_bytes() == expected
    fsck = subprocess.run([find_e2fsck(), "-fn", str(disk)], capture_output=True, text=True)
    native_markers_present = not args.nativeboot or all(marker in output for marker in (
        b"native UART before quiesce",
        b"firmware quiesce complete", b"high kernel alias passed", b"boot handoff validated",
        b"entering kernel", b"Entering WindowServer main loop"))
    if (b"Detected RDB partition, DosType 444f5303" not in output or b"Detected RDB partition, DosType 4c4e5800" not in output
            or not native_markers_present
            or not typed or not file_matches or fsck.returncode != 0
            or b"KERNEL PANIC" in output or b"CRASH:" in output):
        raise SystemExit(
            f"Pegasos 2 RDB card boot failed: typed={typed}, native markers={native_markers_present}, "
            f"file matches={file_matches}, e2fsck={fsck.returncode}\n"
            + fsck.stdout[-1000:] + output.decode(errors="replace")[-3500:] + errors.decode(errors="replace")[-1000:]
        )
    print(f"Pegasos 2 booted the desktop from the RDB card's ext2 partition and wrote {len(expected)} bytes with a clean filesystem")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", required=True)
    parser.add_argument("--loader", type=Path, required=True)
    parser.add_argument("--user-loader", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--cpu", default="750cxe")
    parser.add_argument("--memory", type=int, default=512)
    parser.add_argument("--timeout", type=int, default=300)
    parser.add_argument("--nativeboot", action="store_true")
    args = parser.parse_args()
    source = Path(__file__).parents[3]
    with tempfile.TemporaryDirectory(prefix="powerpc-card-boot-", dir="/tmp") as directory:
        work = Path(directory)
        root = test_desktop.create_root(source, args.build_dir, args.user_loader, work)
        disk = test_desktop.create_disk(root, work)
        card = work / "card.img"
        tools = source / "Meta/PowerPC"
        subprocess.run([sys.executable, "-I", str(tools / "make-pegasos-card.py"), "card", "--loader", str(args.loader),
                        "--root", str(disk), "--output", str(card)], check=True, capture_output=True)
        # SmartFirmware loads serenity.elf from the FFS boot partition; make sure it reads back intact.
        loader_copy = work / "serenity.elf"
        subprocess.run([sys.executable, "-I", str(tools / "amiga-disk.py"), "extract", str(card), "0", "serenity.elf", str(loader_copy)],
                       check=True, capture_output=True)
        if loader_copy.read_bytes() != args.loader.read_bytes():
            raise SystemExit("The card's FFS boot partition does not hold the loader intact")
        boot_and_write(args, card, work)


if __name__ == "__main__":
    main()
