#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

import argparse
from contextlib import nullcontext
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import tempfile
import time

sys.dont_write_bytecode = True
import test_window_server as graphical_boot


def create_root(source, build, user_loader, work):
    root = graphical_boot.create_root(source, build, user_loader, work)
    executables = {
        "bin/Clipboard": "Userland/Services/Clipboard/Clipboard",
        "bin/ConfigServer": "Userland/Services/ConfigServer/ConfigServer",
        "bin/LaunchServer": "Userland/Services/LaunchServer/LaunchServer",
        "bin/Taskbar": "Userland/Services/Taskbar/Taskbar",
        "bin/FileManager": "Userland/Applications/FileManager/FileManager",
        "bin/Terminal": "Userland/Applications/Terminal/Terminal",
        "bin/TerminalSettings": "Userland/Applications/TerminalSettings/TerminalSettings",
        "bin/Shell": "Userland/Shell/Shell",
        "bin/FileOperation": "Userland/Services/FileOperation/FileOperation",
        "bin/utmpupdate": "Userland/Utilities/utmpupdate",
    }
    for destination, executable in executables.items():
        shutil.copy2(build / executable, root / destination)

    shutil.copytree(source / "Base/res/apps", root / "res/apps")
    for filename in ("LaunchServer.ini", "FileIconProvider.ini"):
        shutil.copy2(source / "Base/etc" / filename, root / "etc" / filename)
    (root / "root/.config").mkdir(parents=True)
    (root / "root/Desktop").mkdir()
    (root / "root/Desktop/Terminal").symlink_to("/bin/Terminal")
    (root / "root/.config/Taskbar.ini").write_text(
        "[QuickLaunch_Entries]\nFileManager=0:FileManager.af\nTerminal=1:Terminal.af\n"
    )
    (root / "var/run").mkdir(parents=True)
    (root / "var/run/utmp").write_text("{}")
    (root / "etc/fstab").write_text(
        "/etc\t/etc\tbind\timmutable,bind,nodev,nosuid\n"
        "/root\t/root\tbind\timmutable,bind,nodev,nosuid\n"
        "/var\t/var\tbind\timmutable,bind,nodev,nosuid\n"
    )
    (root / "etc/SystemServer.ini").write_text(
        "[WindowServer]\n"
        "Socket=/tmp/portal/window,/tmp/portal/wm\n"
        "SocketPermissions=660\n"
        "Priority=high\n"
        "KeepAlive=false\n"
        "StdIO=/dev/tty0\n"
        "Environment=HOME=/root\n"
        "SystemModes=graphical\n"
        "\n[ConfigServer]\n"
        "Socket=/tmp/session/%sid/portal/config\n"
        "SocketPermissions=600\n"
        "Environment=HOME=/root\n"
        "\n[Clipboard]\n"
        "Socket=/tmp/session/%sid/portal/clipboard\n"
        "SocketPermissions=600\n"
        "Environment=HOME=/root\n"
        "\n[LaunchServer]\n"
        "Socket=/tmp/session/%sid/portal/launch\n"
        "SocketPermissions=600\n"
        "Lazy=true\n"
        "Environment=HOME=/root\n"
        "\n[Taskbar]\n"
        "KeepAlive=false\n"
        "Environment=HOME=/root\n"
        "\n[Desktop]\n"
        "Executable=/bin/FileManager\n"
        "Arguments=--desktop\n"
        "KeepAlive=false\n"
        "Environment=HOME=/root\n"
        "\n[Terminal]\n"
        "KeepAlive=false\n"
        "Environment=HOME=/root\n"
    )
    return root


def screenshot_has_desktop(path):
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
    if len(pixels) != 1024 * 768 * 3:
        return False

    def pixel(x, y):
        offset = (y * 1024 + x) * 3
        return pixels[offset:offset + 3]

    background = b"\x50\x50\x50"
    if pixel(100, 700) != background or pixel(100, 750) == background:
        return False
    taskbar_control_pixels = sum(pixel(x, y) != background and max(pixel(x, y)) < 100
                                 for y in range(742, 767) for x in range(0, 240))
    if taskbar_control_pixels < 50:
        return False
    desktop_icon_pixels = sum(pixel(x, y) != background for y in range(8, 75) for x in range(12, 75))
    if desktop_icon_pixels < 200:
        return False
    window_samples = sum(pixel(x, y) != background for y in range(120, 660, 16) for x in range(120, 900, 16))
    return window_samples > 150


def boot_and_check(args, disk, work):
    serial = work / "serial.log"
    screenshot = work / "desktop.ppm"
    with tempfile.TemporaryDirectory(prefix="powerpc-desktop-monitor-", dir="/tmp") as monitor_directory:
        monitor_path = Path(monitor_directory) / "monitor.sock"
        command = [
            args.qemu, "-M", "pegasos2", "-cpu", args.cpu, "-m", str(args.memory),
            "-vga", "none", "-device", "virtio-gpu-pci", "-device", "virtio-keyboard-pci",
            "-device", "virtio-mouse-pci", "-kernel", str(args.loader), "-append", "root=lun3:0:0",
            "-drive", f"if=none,format=raw,id=virtio_disk,file={disk}",
            "-device", "virtio-blk-pci,drive=virtio_disk",
            "-object", "rng-random,id=rng0,filename=/dev/urandom", "-device", "virtio-rng-pci,rng=rng0",
            "-display", "none", "-serial", f"file:{serial}",
            "-monitor", f"unix:{monitor_path},server=on,wait=off", "-no-reboot",
        ]
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        passed = False
        try:
            deadline = time.monotonic() + 420
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
                    if (b"KERNEL PANIC" in output or b"VERIFICATION FAILED" in output
                            or b"Exiting with runtime error" in output or b"CRASH:" in output
                            or b"Rejecting path '/dev/beep'" in output):
                        break
                    if b"Entering WindowServer main loop" in output and b"Shell(" in output:
                        graphical_boot.monitor_command(monitor, f"screendump {screenshot}")
                        if screenshot_has_desktop(screenshot):
                            passed = True
                            break
                    time.sleep(1)
        finally:
            process.terminate()
            _, errors = process.communicate(timeout=5)

    output = serial.read_bytes() if serial.exists() else b""
    if (not passed or b"KERNEL PANIC" in output or b"VERIFICATION FAILED" in output
            or b"Exiting with runtime error" in output or b"CRASH:" in output
            or b"Rejecting path '/dev/beep'" in output):
        raise SystemExit("Pegasos 2 desktop boot failed:\n"
                         + output.decode(errors="replace")[-4500:]
                         + errors.decode(errors="replace")[-1000:])
    print(f"Pegasos 2 Taskbar, desktop icon, and Terminal window appeared: {screenshot}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", required=True)
    parser.add_argument("--loader", type=Path, required=True)
    parser.add_argument("--user-loader", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--memory", type=int, default=1024)
    parser.add_argument("--cpu", default="750cxe")
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()

    source = Path(__file__).parents[3]
    if args.output_dir:
        args.output_dir.mkdir(parents=True, exist_ok=True)
        directory_context = nullcontext(tempfile.mkdtemp(prefix="powerpc-desktop-", dir=args.output_dir))
    else:
        directory_context = tempfile.TemporaryDirectory(prefix="powerpc-desktop-", dir="/tmp")
    with directory_context as directory:
        work = Path(directory)
        root = create_root(source, args.build_dir, args.user_loader, work)
        disk = graphical_boot.create_disk(graphical_boot.find_mke2fs(), root, work)
        boot_and_check(args, disk, work)


if __name__ == "__main__":
    main()
