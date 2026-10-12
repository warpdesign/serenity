#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

import argparse
from contextlib import nullcontext
from pathlib import Path
import re
import shutil
import socket
import subprocess
import tempfile
import time


def find_mke2fs():
    mke2fs = shutil.which("mke2fs")
    if mke2fs:
        return mke2fs
    homebrew_mke2fs = Path("/opt/homebrew/opt/e2fsprogs/sbin/mke2fs")
    if homebrew_mke2fs.exists():
        return str(homebrew_mke2fs)
    raise SystemExit("mke2fs is required for the PowerPC graphical boot test")


def create_root(source, build, user_loader, work, omit_fonts=False):
    root = work / "root"
    for directory in ("bin", "usr/lib", "etc", "res", "proc", "sys", "dev", "tmp", "root", "home", "var"):
        (root / directory).mkdir(parents=True, exist_ok=True)

    executables = {
        "init": build / "Userland/Utilities/init",
        "bin/mount": build / "Userland/Utilities/mount",
        "bin/DeviceMapper": build / "Userland/Services/DeviceMapper/DeviceMapper",
        "bin/SystemServer": build / "Userland/Services/SystemServer/SystemServer",
        "bin/WindowServer": build / "Userland/Services/WindowServer/WindowServer",
        "bin/keymap": build / "Userland/Utilities/keymap",
    }
    for destination, executable in executables.items():
        shutil.copy2(executable, root / destination)
    shutil.copy2(user_loader, root / "usr/lib/Loader.so")
    for library in (build / "Userland/Libraries").glob("**/*.so*"):
        if library.is_file():
            shutil.copy2(library, root / "usr/lib" / library.name)

    for directory in ("fonts", "themes", "icons", "cursor-themes", "color-schemes", "wallpapers", "keymaps"):
        if directory == "fonts" and omit_fonts:
            continue
        shutil.copytree(source / "Base/res" / directory, root / "res" / directory)
    for filename in ("WindowServer.ini", "Keyboard.ini", "DeviceMapper.ini", "passwd", "group"):
        shutil.copy2(source / "Base/etc" / filename, root / "etc" / filename)
    (root / "etc/fstab").write_text("/etc\t/etc\tbind\timmutable,bind,nodev,nosuid\n")
    (root / "etc/SystemServer.ini").write_text(
        "[WindowServer]\n"
        "Socket=/tmp/portal/window,/tmp/portal/wm\n"
        "SocketPermissions=660\n"
        "Priority=high\n"
        "KeepAlive=false\n"
        "StdIO=/dev/tty0\n"
        "SystemModes=graphical\n"
    )
    return root


def create_disk(mke2fs, root, work):
    root_bytes = sum(path.stat().st_size for path in root.rglob("*") if path.is_file())
    megabyte = 1024 * 1024
    size_bytes = max(256 * megabyte, root_bytes * 5 // 4 + 64 * megabyte)
    size_bytes = ((size_bytes + 64 * megabyte - 1) // (64 * megabyte)) * 64 * megabyte
    disk = work / "root.ext2"
    with disk.open("wb") as file:
        file.truncate(size_bytes)
    subprocess.run([mke2fs, "-q", "-F", "-t", "ext2", "-O", "none", "-b", "4096", "-d", str(root), str(disk)], check=True)
    with disk.open("r+b") as file:
        file.seek(510)
        file.write(b"\x55\xaa")
    return disk


def monitor_command(monitor, command):
    monitor.sendall(command.encode() + b"\n")
    output = b""
    while not output.endswith(b"(qemu) "):
        chunk = monitor.recv(16384)
        if not chunk:
            raise RuntimeError("QEMU monitor closed during graphical boot test")
        output += chunk
    return output


def screenshot_is_window_server_frame(path):
    if not path.exists():
        return False
    with path.open("rb") as image:
        if image.readline().strip() != b"P6":
            return False
        dimensions = image.readline()
        while dimensions.startswith(b"#"):
            dimensions = image.readline()
        if dimensions.split() != [b"1024", b"768"] or image.readline().strip() != b"255":
            return False
        pixels = image.read()
    return (pixels.count(b"\x50\x50\x50") > 100_000
            and pixels.count(b"\xff\xff\xff") >= 20
            and pixels.count(b"\x00\x00\x00") >= 20)


def boot_and_check(args, disk, work):
    serial = work / "serial.log"
    monitor_directory = tempfile.TemporaryDirectory(prefix="powerpc-window-server-monitor-", dir="/tmp")
    monitor_path = Path(monitor_directory.name) / "monitor.sock"
    screenshot = work / "window-server.ppm"
    command = [
        args.qemu, "-M", "pegasos2", "-cpu", args.cpu, "-m", str(args.memory),
        "-vga", "none", "-device", "virtio-gpu-pci", "-kernel", str(args.loader),
        "-append", "root=lun3:0:0", "-drive", f"if=none,format=raw,id=virtio_disk,file={disk}",
        "-device", "virtio-blk-pci,drive=virtio_disk", "-object", "rng-random,id=rng0,filename=/dev/urandom",
        "-device", "virtio-rng-pci,rng=rng0", "-display", "none", "-serial", f"file:{serial}",
        "-monitor", f"unix:{monitor_path},server=on,wait=off", "-no-reboot",
    ]
    process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    passed = False
    block_stats = b""
    errors = b""
    try:
        deadline = time.monotonic() + 330
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
                if b"KERNEL PANIC" in output or b"VERIFICATION FAILED" in output:
                    break
                if b"Entering WindowServer main loop" in output:
                    monitor_command(monitor, f"screendump {screenshot}")
                    if screenshot_is_window_server_frame(screenshot):
                        block_stats = monitor_command(monitor, "info blockstats")
                        passed = True
                        break
                time.sleep(0.5)
    finally:
        process.terminate()
        _, errors = process.communicate(timeout=5)
        monitor_directory.cleanup()

    output = serial.read_bytes() if serial.exists() else b""
    if (not passed or b"Read system_mode: graphical" not in output
            or b"KERNEL PANIC" in output or b"VERIFICATION FAILED" in output):
        raise SystemExit("Pegasos 2 WindowServer graphical boot failed:\n"
                         + output.decode(errors="replace")[-4500:]
                         + errors.decode(errors="replace")[-1000:])
    match = re.search(rb"virtio_disk: rd_bytes=(\d+).*?rd_operations=(\d+)", block_stats)
    io_summary = f", {int(match[2])} reads for {int(match[1])} bytes" if match else ""
    print(f"Pegasos 2 WindowServer painted 1024x768 VirtIO GPU frame{io_summary}: {screenshot}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", required=True)
    parser.add_argument("--loader", type=Path, required=True)
    parser.add_argument("--user-loader", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--memory", type=int, default=512)
    parser.add_argument("--cpu", default="750cxe")
    parser.add_argument("--omit-fonts", action="store_true")
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()

    source = Path(__file__).parents[3]
    if args.output_dir:
        args.output_dir.mkdir(parents=True, exist_ok=True)
        directory_context = nullcontext(tempfile.mkdtemp(prefix="powerpc-window-server-", dir=args.output_dir))
    else:
        directory_context = tempfile.TemporaryDirectory(prefix="powerpc-window-server-", dir="/tmp")
    with directory_context as directory:
        work = Path(directory)
        root = create_root(source, args.build_dir, args.user_loader, work, args.omit_fonts)
        disk = create_disk(find_mke2fs(), root, work)
        boot_and_check(args, disk, work)


if __name__ == "__main__":
    main()
