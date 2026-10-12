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
import sys
import tempfile
import time

sys.dont_write_bytecode = True
import test_desktop
import test_window_server


def prepare_root(source, build, user_loader, work):
    root = test_desktop.create_root(source, build, user_loader, work)
    programs = {
        "Browser": "Userland/Applications/Browser/Browser",
        "BrowserSettings": "Userland/Applications/BrowserSettings/BrowserSettings",
        "RequestServer": "Userland/Services/RequestServer/RequestServer",
        "WebContent": "Userland/Services/WebContent/WebContent",
        "WebWorker": "Userland/Services/WebWorker/WebWorker",
        "AudioServer": "Userland/Services/AudioServer/AudioServer",
        "LookupServer": "Userland/Services/LookupServer/LookupServer",
        "NetworkServer": "Userland/Services/NetworkServer/NetworkServer",
        "DHCPClient": "Userland/Services/DHCPClient/DHCPClient",
        "SQLServer": "Userland/Services/SQLServer/SQLServer",
        "FileSystemAccessServer": "Userland/Services/FileSystemAccessServer/FileSystemAccessServer",
    }
    for name, location in programs.items():
        shutil.copy2(build / location, root / "bin" / name)
    for name in ("Browser", "BrowserSettings"):
        shutil.copy2(source / "Base/res/apps" / f"{name}.af", root / "res/apps" / f"{name}.af")
    for name in ("html", "ladybird", "emoji", "color-palettes"):
        shutil.copytree(source / "Base/res" / name, root / "res" / name)
    for name in ("BrowserContentFilters.txt", "BrowserAutoplayAllowlist.txt"):
        shutil.copy2(source / "Base/home/anon/.config" / name, root / "home/anon/.config" / name)
    certificates = build / "Root/etc/cacert.pem"
    if certificates.exists():
        shutil.copy2(certificates, root / "etc/cacert.pem")
    else:
        (root / "etc/cacert.pem").touch()
    (root / "etc/cacert.pem").chmod(0o644)
    shutil.copy2(source / "Base/etc/hosts", root / "etc/hosts")
    (root / "etc/LookupServer.ini").write_text("[DNS]\nNameservers=10.0.2.3\nEnableServer=false\n")
    shutil.copy2(source / "Base/etc/Network.ini", root / "etc/Network.ini")
    (root / "home/anon/Downloads").mkdir()
    (root / "home/anon/browser-smoke.html").write_text(
        "<!doctype html><title>PowerPC browser smoke</title>"
        "<style>html,body { width: 100%; height: 100%; margin: 0; background: #e02030; }</style>"
        "<h1>PowerPC browser smoke</h1>\n"
    )
    (root / "home/anon/network-audit.sh").write_text(
        "/bin/sleep 12\n"
        "/bin/ifconfig > /home/anon/network-autoconfig.log 2>&1\n"
        "/bin/route >> /home/anon/network-autoconfig.log 2>&1\n"
        "/bin/ping -c 1 -W 2 10.0.2.2 >> /home/anon/network-autoconfig.log 2>&1\n"
        "/bin/host google.com >> /home/anon/network-autoconfig.log 2>&1\n"
        "/bin/date -u >> /home/anon/network-autoconfig.log 2>&1\n"
    )
    with (root / "etc/SystemServer.ini").open("a") as config:
        config.write(
            "\n[NetworkServer]\nUser=root\nKeepAlive=false\nSystemModes=graphical\n"
            "\n[LookupServer]\nSocket=/tmp/portal/lookup\nSocketPermissions=660\nUser=lookup\nKeepAlive=true\nSystemModes=graphical\n"
        )
    with (root / "etc/SystemServerUser.ini").open("a") as config:
        config.write(
            "\n[AudioServer]\nSocket=/tmp/session/%sid/portal/audio,/tmp/session/%sid/portal/audiomanager\n"
            "SocketPermissions=600\nKeepAlive=false\n"
            "\n[RequestServer]\nSocket=/tmp/session/%sid/portal/request\nSocketPermissions=600\nLazy=true\nMultiInstance=true\nAcceptSocketConnections=true\n"
            "\n[WebContent]\nSocket=/tmp/session/%sid/portal/webcontent\nSocketPermissions=600\nLazy=true\nMultiInstance=true\nAcceptSocketConnections=true\n"
            "\n[WebWorker]\nSocket=/tmp/session/%sid/portal/webworker\nSocketPermissions=600\nLazy=true\nMultiInstance=true\nAcceptSocketConnections=true\n"
            "\n[SQLServer]\nSocket=/tmp/session/%sid/portal/sql\nSocketPermissions=600\nLazy=true\nKeepAlive=true\n"
            "\n[FileSystemAccessServer]\nSocket=/tmp/session/%sid/portal/filesystemaccess\nSocketPermissions=660\nLazy=true\nMultiInstance=true\nAcceptSocketConnections=true\n"
            "\n[BrowserSmoke]\nExecutable=/bin/Browser\nArguments=file:///home/anon/browser-smoke.html\nKeepAlive=false\nEnvironment=HOME=/home/anon\n"
            "\n[NetworkAudit]\nExecutable=/bin/Shell\nArguments=/home/anon/network-audit.sh\nKeepAlive=false\nEnvironment=HOME=/home/anon\n"
        )
    return root


def red_sample_count(path):
    if not path.exists():
        return 0
    with path.open("rb") as screenshot:
        if screenshot.readline().strip() != b"P6":
            return 0
        dimensions = screenshot.readline()
        while dimensions.startswith(b"#"):
            dimensions = screenshot.readline()
        if dimensions.split() != [b"1024", b"768"] or screenshot.readline().strip() != b"255":
            return 0
        pixels = screenshot.read()
    if len(pixels) != 1024 * 768 * 3:
        return 0
    count = 0
    for y in range(120, 680, 4):
        for x in range(100, 920, 4):
            offset = (y * 1024 + x) * 3
            red, green, blue = pixels[offset:offset + 3]
            if red > 180 and green < 80 and blue < 100:
                count += 1
    return count


def boot_and_check(args, disk, work):
    serial = work / "serial.log"
    screenshot = work / "browser.ppm"
    with tempfile.TemporaryDirectory(prefix="powerpc-browser-monitor-", dir="/tmp") as monitor_directory:
        monitor_path = Path(monitor_directory) / "monitor.sock"
        command = [
            args.qemu, "-M", "pegasos2", "-cpu", args.cpu, "-m", str(args.memory),
            "-vga", "none", "-device", "virtio-gpu-pci", "-device", "virtio-keyboard-pci",
            "-device", "virtio-mouse-pci", "-kernel", str(args.loader), "-append", "root=lun3:0:0",
            "-drive", f"if=none,format=raw,id=virtio_disk,file={disk}",
            "-device", "virtio-blk-pci,drive=virtio_disk",
            "-netdev", "user,id=net0", "-device", "virtio-net-pci,netdev=net0",
            "-object", "rng-random,id=rng0,filename=/dev/urandom", "-device", "virtio-rng-pci,rng=rng0",
            "-display", "none", "-serial", f"file:{serial}",
            "-monitor", f"unix:{monitor_path},server=on,wait=off", "-no-reboot",
        ]
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        browser_visible = False
        network_audit_complete = False
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
                    if b"KERNEL PANIC" in output or b"VERIFICATION FAILED" in output:
                        break
                    if b"Service NetworkAudit has exited with exit code 0" in output:
                        network_audit_complete = True
                    if not browser_visible and b"Entering WindowServer main loop" in output and b"Browser(" in output:
                        test_window_server.monitor_command(monitor, f"screendump {screenshot}")
                        browser_visible = red_sample_count(screenshot) > 5000
                    if browser_visible and b"WebContent(" in output and network_audit_complete:
                        time.sleep(2)
                        break
                    time.sleep(0.5)
        finally:
            process.terminate()
            _, errors = process.communicate(timeout=5)
    output = serial.read_bytes() if serial.exists() else b""
    network_log_path = work / "network-autoconfig.log"
    subprocess.run(
        [test_desktop.find_debugfs(), "-R", f"dump /home/anon/network-autoconfig.log {network_log_path}", str(disk)],
        capture_output=True, text=True,
    )
    network_log = network_log_path.read_text(errors="replace") if network_log_path.exists() else ""
    ethernet_section = network_log.split("\nloop:", 1)[0]
    address = re.search(r"(?m)^\tipv4: ([0-9.]+)$", ethernet_section)
    guest_timestamp = re.search(r"(?m)^([0-9]{10,})$", network_log)
    clock_ready = guest_timestamp is not None and abs(int(guest_timestamp.group(1)) - int(time.time())) < 120
    network_ready = (address is not None and address.group(1) not in ("0.0.0.0", "127.0.0.1")
                     and "Pong from 10.0.2.2" in network_log and "Kernel IP routing table" in network_log
                     and "google.com is " in network_log and clock_ready)
    if (not browser_visible or b"WebContent(" not in output or not network_audit_complete or not network_ready
            or b"Exiting with runtime error" in output or b"KERNEL PANIC" in output):
        raise SystemExit(
            f"Pegasos 2 Browser smoke failed: red samples={red_sample_count(screenshot)}, "
            f"WebContent started={b'WebContent(' in output}, network ready={network_ready}, "
            f"network audit complete={network_audit_complete}, clock ready={clock_ready}, screenshot={screenshot}\n"
            + network_log[-1500:] + "\n"
            + output.decode(errors="replace")[-4500:] + errors.decode(errors="replace")[-500:]
        )
    print(f"Pegasos 2 Browser rendered local HTML and DHCP configured {address.group(1)}; red samples={red_sample_count(screenshot)}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", required=True)
    parser.add_argument("--loader", type=Path, required=True)
    parser.add_argument("--user-loader", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--memory", type=int, default=512)
    parser.add_argument("--cpu", default="750cxe")
    parser.add_argument("--timeout", type=int, default=90)
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()
    source = Path(__file__).parents[3]
    parent = args.output_dir or Path("/tmp")
    parent.mkdir(parents=True, exist_ok=True)
    directory_context = (nullcontext(tempfile.mkdtemp(prefix="powerpc-browser-", dir=parent))
                         if args.output_dir else tempfile.TemporaryDirectory(prefix="powerpc-browser-", dir=parent))
    with directory_context as directory:
        work = Path(directory)
        root = prepare_root(source, args.build_dir, args.user_loader, work)
        disk = test_desktop.create_disk(root, work)
        boot_and_check(args, disk, work)


if __name__ == "__main__":
    main()
