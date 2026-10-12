#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

import argparse
from contextlib import nullcontext
import json
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


def prepare_root(source, build, user_loader, work):
    root = test_desktop.create_root(source, build, user_loader, work)
    shutil.copy2(build / "Userland/Applications/PixelPaint/PixelPaint", root / "bin/PixelPaint")
    shutil.copy2(source / "Base/res/apps/PixelPaint.af", root / "res/apps/PixelPaint.af")
    shutil.copytree(source / "Base/res/color-palettes", root / "res/color-palettes")
    with (root / "etc/SystemServerUser.ini").open("a") as config:
        config.write(
            "\n[PixelPaintSmoke]\n"
            "Executable=/bin/PixelPaint\nKeepAlive=false\nEnvironment=HOME=/home/anon\n"
        )
    return root


def pixels(path):
    with path.open("rb") as screenshot:
        if screenshot.readline().strip() != b"P6":
            raise ValueError(f"Invalid screenshot: {path}")
        dimensions = screenshot.readline()
        while dimensions.startswith(b"#"):
            dimensions = screenshot.readline()
        if dimensions.split() != [b"1024", b"768"] or screenshot.readline().strip() != b"255":
            raise ValueError(f"Unexpected screenshot size: {path}")
        data = screenshot.read()
    if len(data) != 1024 * 768 * 3:
        raise ValueError(f"Incomplete screenshot: {path}")
    return data


def changed_canvas_pixels(before, after):
    old = pixels(before)
    new = pixels(after)
    changed = 0
    for x in range(400, 620):
        offset = (350 * 1024 + x) * 3
        changed += old[offset:offset + 3] != b"\x00\x00\x00" and new[offset:offset + 3] == b"\x00\x00\x00"
    return changed


def boot_and_check(args, disk, work):
    serial = work / "serial.log"
    before = work / "pixel-paint-before.ppm"
    after = work / "pixel-paint-after.ppm"
    save_dialog = work / "pixel-paint-save-dialog.ppm"
    saved_project = work / "Untitled.pp"
    with tempfile.TemporaryDirectory(prefix="powerpc-pixel-paint-monitor-") as monitor_directory:
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
        painted_pixels = 0
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
                    if b"KERNEL PANIC" in output or b"CRASH:" in output or b"Exiting with runtime error" in output:
                        break
                    if b"Entering WindowServer main loop" in output and b"PixelPaint(" in output:
                        test_window_server.monitor_command(monitor, f"screendump {before}")
                        if not test_desktop.screenshot_has_desktop(before):
                            time.sleep(0.5)
                            continue
                        time.sleep(2)
                        test_window_server.monitor_command(monitor, f"screendump {before}")
                        test_window_server.monitor_command(monitor, "mouse_move -112 -34")
                        test_window_server.monitor_command(monitor, "mouse_button 1")
                        for _ in range(8):
                            test_window_server.monitor_command(monitor, "mouse_move 25 0")
                            time.sleep(0.1)
                        test_window_server.monitor_command(monitor, "mouse_button 0")
                        test_window_server.monitor_command(monitor, "mouse_move 80 80")
                        time.sleep(1)
                        test_window_server.monitor_command(monitor, f"screendump {after}")
                        painted_pixels = changed_canvas_pixels(before, after)
                        test_window_server.monitor_command(monitor, "sendkey ctrl-s")
                        time.sleep(2)
                        test_window_server.monitor_command(monitor, f"screendump {save_dialog}")
                        test_window_server.monitor_command(monitor, "sendkey ret")
                        time.sleep(20)
                        break
                    time.sleep(0.5)
        finally:
            process.terminate()
            _, errors = process.communicate(timeout=5)

    output = serial.read_bytes() if serial.exists() else b""
    subprocess.run(
        [test_desktop.find_debugfs(), "-R", f"dump /home/anon/Untitled.pp {saved_project}", str(disk)],
        capture_output=True, text=True,
    )
    project = None
    if saved_project.exists():
        try:
            project = json.loads(saved_project.read_text())
        except (OSError, ValueError):
            pass
    project_saved = (isinstance(project, dict) and project.get("width") == 510 and project.get("height") == 356
                     and isinstance(project.get("layers"), list) and len(project["layers"]) == 1)
    if (painted_pixels < 150 or not project_saved or b"KERNEL PANIC" in output or b"CRASH:" in output
            or b"Exiting with runtime error" in output):
        raise SystemExit(
            f"Pegasos 2 PixelPaint drawing or save failed: painted pixels={painted_pixels}, "
            f"project saved={project_saved}, before={before}, after={after}, save dialog={save_dialog}\n"
            + output.decode(errors="replace")[-3500:] + errors.decode(errors="replace")[-1000:]
        )
    print(f"Pegasos 2 PixelPaint painted {painted_pixels} canvas pixels and saved {saved_project.stat().st_size} bytes")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", required=True)
    parser.add_argument("--loader", type=Path, required=True)
    parser.add_argument("--user-loader", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--cpu", default="750cxe")
    parser.add_argument("--memory", type=int, default=512)
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()
    source = Path(__file__).parents[3]
    parent = args.output_dir or Path("/tmp")
    parent.mkdir(parents=True, exist_ok=True)
    directory_context = (nullcontext(tempfile.mkdtemp(prefix="powerpc-pixel-paint-", dir=parent))
                         if args.output_dir else tempfile.TemporaryDirectory(prefix="powerpc-pixel-paint-", dir=parent))
    with directory_context as directory:
        work = Path(directory)
        root = prepare_root(source, args.build_dir, args.user_loader, work)
        disk = test_desktop.create_disk(root, work)
        boot_and_check(args, disk, work)


if __name__ == "__main__":
    main()
