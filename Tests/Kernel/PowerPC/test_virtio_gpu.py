#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

import argparse
from pathlib import Path
import re
import shutil
import socket
import subprocess
import tempfile
import time


def read_monitor(monitor, command):
    monitor.sendall(command.encode() + b"\n")
    output = b""
    while not output.endswith(b"(qemu) "):
        chunk = monitor.recv(16384)
        if not chunk:
            raise RuntimeError("QEMU monitor closed during VirtIO GPU test")
        output += chunk
    return output


def image_size(path):
    if not path.exists():
        return None
    with path.open("rb") as image:
        if image.readline().strip() != b"P6":
            return None
        line = image.readline()
        while line.startswith(b"#"):
            line = image.readline()
        dimensions = line.split()
        if len(dimensions) != 2:
            return None
        return tuple(int(value) for value in dimensions)


def image_has_console_colors(path):
    with path.open("rb") as image:
        if image.readline().strip() != b"P6":
            return False
        line = image.readline()
        while line.startswith(b"#"):
            line = image.readline()
        if line.split() != [b"1024", b"768"] or image.readline().strip() != b"255":
            return False
        pixels = image.read()
        return (pixels.count(b"\xff\x00\x00") >= 10
                and pixels.count(b"\x00\xff\x00") >= 10
                and pixels.count(b"\x00\x00\xff") >= 10)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", required=True)
    parser.add_argument("--loader", required=True)
    parser.add_argument("--cross-compile", required=True)
    parser.add_argument("--memory", type=int, default=512)
    parser.add_argument("--cpu")
    args = parser.parse_args()

    mke2fs = shutil.which("mke2fs")
    if mke2fs is None:
        homebrew_mke2fs = Path("/opt/homebrew/opt/e2fsprogs/sbin/mke2fs")
        if homebrew_mke2fs.exists():
            mke2fs = str(homebrew_mke2fs)
    if mke2fs is None:
        raise SystemExit("mke2fs is required for the PowerPC VirtIO GPU test")
    debugfs = shutil.which("debugfs")
    if debugfs is None:
        homebrew_debugfs = Path("/opt/homebrew/opt/e2fsprogs/sbin/debugfs")
        if homebrew_debugfs.exists():
            debugfs = str(homebrew_debugfs)
    if debugfs is None:
        raise SystemExit("debugfs is required for the PowerPC VirtIO GPU color test")

    syscall_header = Path(__file__).parents[3] / "Kernel/API/Syscall.h"
    syscall_names = re.findall(r"^\s*S\((\w+),", syscall_header.read_text(), re.MULTILINE)

    with tempfile.TemporaryDirectory(prefix="serenity-powerpc-virtio-gpu-") as directory:
        work = Path(directory)
        fixtures = Path(__file__).parent / "Fixtures"
        object_file = work / "wait.o"
        executable = work / "init"
        subprocess.run([
            args.cross_compile + "gcc", "-m32", "-msdata=none", "-ffreestanding", "-fno-pic", "-fno-pie",
            f"-DSYSCALL_OPEN={syscall_names.index('open')}",
            f"-DSYSCALL_WRITE={syscall_names.index('write')}",
            "-c", str(fixtures / "GPUConsoleColor.S"), "-o", str(object_file),
        ], check=True)
        subprocess.run([
            args.cross_compile + "ld", "-m", "elf32ppcserenity", "-shared", "--no-dynamic-linker",
            "-T", str(fixtures / "UserLoop.ld"), "--entry=_start", str(object_file), "-o", str(executable),
        ], check=True)

        root = work / "root"
        root.mkdir()
        (root / "dev").mkdir()
        shutil.copy2(executable, root / "init")
        disk = work / "root.ext2"
        with disk.open("wb") as file:
            file.truncate(16 * 1024 * 1024)
        subprocess.run([mke2fs, "-q", "-F", "-t", "ext2", "-O", "none", "-b", "1024", "-d", str(root), str(disk)], check=True)
        subprocess.run(
            [debugfs, "-w", "-f", "-", str(disk)],
            input="cd /dev\nmknod tty0 c 35 0\nset_inode_field /dev/tty0 mode 020666\n",
            check=True, capture_output=True, text=True,
        )
        device = subprocess.run([debugfs, "-R", "stat /dev/tty0", str(disk)], check=True, capture_output=True, text=True)
        if "Type: character special" not in device.stdout:
            raise SystemExit("Failed to create /dev/tty0 in VirtIO GPU test image")
        with disk.open("r+b") as file:
            file.seek(510)
            file.write(b"\x55\xaa")

        serial = work / "serial.log"
        monitor_path = work / "monitor.sock"
        screenshot = work / "scanout.ppm"
        command = [
            args.qemu, "-M", "pegasos2", "-m", str(args.memory), "-vga", "none",
            "-device", "virtio-gpu-pci", "-kernel", args.loader,
            "-append", "root=lun3:0:0",
            "-drive", f"if=none,format=raw,id=virtio_disk,file={disk}",
            "-device", "virtio-blk-pci,drive=virtio_disk",
            "-object", "rng-random,id=rng0,filename=/dev/urandom",
            "-device", "virtio-rng-pci,rng=rng0",
            "-display", "none", "-serial", f"file:{serial}",
            "-monitor", f"unix:{monitor_path},server=on,wait=off",
            "-no-reboot", "-icount", "shift=auto", "-trace", "enable=virtio_gpu_cmd_res_create_2d",
        ]
        if args.cpu:
            command.extend(("-cpu", args.cpu))
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        observed_size = None
        observed_colors = False
        try:
            deadline = time.monotonic() + 20
            while not monitor_path.exists() and time.monotonic() < deadline and process.poll() is None:
                time.sleep(0.05)
            with socket.socket(socket.AF_UNIX) as monitor:
                monitor.settimeout(3)
                monitor.connect(str(monitor_path))
                greeting = b""
                while not greeting.endswith(b"(qemu) "):
                    greeting += monitor.recv(16384)
                while time.monotonic() < deadline and process.poll() is None:
                    output = serial.read_bytes() if serial.exists() else b""
                    if b"Failed to get EDID" in output or b"No graphics adapter was initialized" in output or b"KERNEL PANIC" in output:
                        break
                    read_monitor(monitor, f"screendump {screenshot}")
                    observed_size = image_size(screenshot)
                    if observed_size == (1024, 768) and image_has_console_colors(screenshot):
                        observed_colors = True
                        break
                    time.sleep(0.1)
        finally:
            process.terminate()
            _, errors = process.communicate(timeout=5)

        output = serial.read_bytes() if serial.exists() else b""
        if (observed_size != (1024, 768) or not observed_colors or b"VirtIOGPU: Found" not in output
                or not re.search(rb"virtio_gpu_cmd_res_create_2d .*fmt 0x2,", errors)
                or b"Failed to get EDID" in output or b"No graphics adapter was initialized" in output
                or b"KERNEL PANIC" in output):
            raise SystemExit(
                f"Pegasos 2 VirtIO GPU did not render RGB console text in B8G8R8X8 format on a 1024x768 scanout: {observed_size}, colors={observed_colors}\n"
                + output.decode(errors="replace")[-3000:]
                + errors.decode(errors="replace")[-1000:]
            )
    print("Pegasos 2 VirtIO GPU B8G8R8X8 scanout and RGB console colors passed")


if __name__ == "__main__":
    main()
