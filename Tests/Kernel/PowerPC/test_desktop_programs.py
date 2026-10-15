#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

import argparse
from contextlib import nullcontext
from pathlib import Path
import re
import socket
import struct
import subprocess
import sys
import tempfile
import time

sys.dont_write_bytecode = True
import test_desktop
import test_window_server


def prepare_root(args, source, build, user_loader, work, quick):
    root = test_desktop.create_root(source, build, user_loader, work)
    inttypes_probe = root / "bin/InttypesProbe"
    subprocess.run([
        args.compiler, "-m32", "-msecure-plt", "-fPIC", "-fno-stack-protector", "-O2", "-Werror=format",
        "-nostartfiles", "-nodefaultlibs", "-Wl,-e,_start", "-Wl,--dynamic-linker=/usr/lib/Loader.so",
        "-I" + str(source / "Userland/Libraries/LibC"),
        "-L" + str(args.libc.resolve().parent), "-L" + str(args.libsystem.resolve().parent),
        str(source / "Tests/Kernel/PowerPC/Fixtures/InttypesFormat.c"),
        "-lc", "-lsystem", "-lgcc", "-o", str(inttypes_probe),
    ], check=True)
    atomic_probe = root / "bin/Atomic64Probe"
    subprocess.run([
        args.compiler, "-m32", "-msecure-plt", "-fPIC", "-fno-stack-protector", "-O2",
        "-nostartfiles", "-nodefaultlibs", "-Wl,-e,_start", "-Wl,--dynamic-linker=/usr/lib/Loader.so",
        "-L" + str(args.libc.resolve().parent), "-L" + str(args.libsystem.resolve().parent),
        str(source / "Tests/Kernel/PowerPC/Fixtures/Atomic64.c"),
        "-lc", "-lsystem", "-lgcc", "-o", str(atomic_probe),
    ], check=True)
    js_value_probe = root / "bin/JSValueTagProbe"
    subprocess.run([
        args.cxx_compiler, "-std=c++26", "-m32", "-msecure-plt", "-fPIC", "-fno-stack-protector", "-fno-exceptions", "-O2",
        "-nostartfiles", "-nodefaultlibs", "-Wl,-e,_start", "-Wl,--dynamic-linker=/usr/lib/Loader.so",
        "-I" + str(source), "-I" + str(source / "Userland/Libraries"),
        "-L" + str(args.libc.resolve().parent), "-L" + str(args.libsystem.resolve().parent),
        str(source / "Tests/Kernel/PowerPC/Fixtures/ValueTag.cpp"),
        "-lc", "-lsystem", "-lgcc", "-o", str(js_value_probe),
    ], check=True)
    curve_probe = root / "bin/P256CurveProbe"
    subprocess.run([
        args.cxx_compiler, "-std=c++26", "-m32", "-msecure-plt", "-fPIC", "-fno-stack-protector", "-fno-exceptions", "-O2",
        "-nostartfiles", "-nodefaultlibs", "-Wl,-e,_start", "-Wl,--dynamic-linker=/usr/lib/Loader.so",
        "-I" + str(source), "-I" + str(source / "Userland/Libraries"),
        "-L" + str(args.libc.resolve().parent), "-L" + str(args.libsystem.resolve().parent),
        "-L" + str(build / "Userland/Libraries/LibCrypto"),
        str(source / "Tests/Kernel/PowerPC/Fixtures/P256Curve.cpp"),
        "-lcrypto", "-lc", "-lsystem", "-lgcc", "-o", str(curve_probe),
    ], check=True)
    aligned_new_probe = root / "bin/AlignedNewProbe"
    subprocess.run([
        args.cxx_compiler, "-std=c++26", "-m32", "-msecure-plt", "-fPIC", "-fno-stack-protector", "-fno-exceptions", "-O2",
        "-nostartfiles", "-nodefaultlibs", "-Wl,-e,_start", "-Wl,--dynamic-linker=/usr/lib/Loader.so",
        "-L" + str(args.libc.resolve().parent), "-L" + str(args.libsystem.resolve().parent),
        str(source / "Tests/Kernel/PowerPC/Fixtures/AlignedNew.cpp"),
        "-lc", "-lsystem", "-lgcc", "-o", str(aligned_new_probe),
    ], check=True)
    sincos_probe = root / "bin/MathSincosProbe"
    subprocess.run([
        args.compiler, "-m32", "-msecure-plt", "-fPIC", "-fno-stack-protector", "-O2",
        "-nostartfiles", "-nodefaultlibs", "-Wl,-e,_start", "-Wl,--dynamic-linker=/usr/lib/Loader.so",
        "-I" + str(source / "Userland/Libraries/LibC"),
        "-L" + str(args.libc.resolve().parent), "-L" + str(args.libsystem.resolve().parent),
        str(source / "Tests/Kernel/PowerPC/Fixtures/MathSincos.c"),
        "-lc", "-lsystem", "-lgcc", "-o", str(sincos_probe),
    ], check=True)
    elf_ident = b"\x7fELF" + bytes((2, 1, 1, 0, 0)) + bytes(7)
    (root / "home/anon/little-endian-elf").write_bytes(
        elf_ident + struct.pack("<HHIQQQIHHHHHH", 1, 62, 1, 0, 0, 0, 0, 64, 0, 0, 0, 0, 0)
    )
    editor_fixture = root / "home/anon/editor.txt"
    editor_fixture.write_text("Editor test\n")
    editor_fixture.chmod(0o666)

    log = "/home/anon/program-check.log"
    script = [f"echo PROGRAM_CHECK_START > {log}"]
    help_commands = []
    if not quick:
        for name in test_desktop.required_utility_names(source):
            utility_source = source / "Userland/Utilities" / f"{name}.cpp"
            if name == "stty" or "ArgsParser" not in utility_source.read_text():
                continue
            help_commands.append(name)
            script.extend((
                f"echo HELP_BEGIN_{name} >> {log}",
                f"/bin/{name} --help > /dev/null 2>&1",
                f"echo HELP_RESULT_{name}_$? >> {log}",
            ))

    commands = {
        "uname": "/bin/uname -a",
        "lscpu": "/bin/lscpu",
        "ls": "/bin/ls -l /bin/About",
        "native_elf": "/bin/file /bin/About",
        "foreign_elf": "/bin/file /home/anon/little-endian-elf",
        "inttypes": "/bin/InttypesProbe",
        "atomic64": "/bin/Atomic64Probe",
        "aligned_new": "/bin/AlignedNewProbe",
        "sincosf": "/bin/MathSincosProbe",
        "js_value_tag": "/bin/JSValueTagProbe",
        "p256_curve": "/bin/P256CurveProbe",
        "ps": "/bin/ps",
        "lspci": "/bin/lspci",
        "lsusb": "/bin/lsusb",
        "memstat": "/bin/memstat",
        "lsblk": "/bin/lsblk",
        "lsdev": "/bin/lsdev",
        "lsirq": "/bin/lsirq",
        "membench_verify": "/bin/membench --verify",
    }
    if not quick:
        commands.update({
            "stat": "/bin/stat /bin/About",
            "df": "/bin/df",
            "date": "/bin/date -u",
            "ping": "/bin/ping -c 1 -W 1 127.0.0.1",
            "w": "/bin/w",
            "find": "/bin/find /bin -name uname",
            "nproc": "/bin/nproc",
            "whoami": "/bin/whoami",
            "test": "/bin/test 1 = 1",
            "expr": "/bin/expr 1 + 2",
            "cat": "/bin/cat /etc/passwd",
            "head": "/bin/head /etc/passwd",
            "tail": "/bin/tail /etc/passwd",
        })
    for name, command in commands.items():
        script.extend((
            f"echo COMMAND_BEGIN_{name} >> {log}",
            f"{command} >> {log} 2>&1",
            f"echo COMMAND_RESULT_{name}_$? >> {log}",
        ))
    script.append(f"echo PROGRAM_CHECK_COMPLETE >> {log}")
    (root / "home/anon/program-check.sh").write_text("\n".join(script) + "\n")
    with (root / "etc/SystemServerUser.ini").open("a") as config:
        config.write(
            "\n[ProgramCheck]\n"
            "Executable=/bin/Shell\n"
            "Arguments=/home/anon/program-check.sh\n"
            "KeepAlive=false\n"
            "Environment=HOME=/home/anon\n"
        )
    (root / "root/network-traffic.sh").write_text(
        "/bin/ifconfig -a ep0s5 --ipv4 10.0.2.15 --mask 255.255.255.0 > /home/anon/network-traffic.log 2>&1\n"
        "echo CONFIG_STATUS_$? >> /home/anon/network-traffic.log\n"
        "/bin/ping -c 1 -W 2 10.0.2.2 >> /home/anon/network-traffic.log 2>&1\n"
        "echo PING_STATUS_$? >> /home/anon/network-traffic.log\n"
        "/bin/ifconfig >> /home/anon/network-traffic.log 2>&1\n"
        "echo IFCONFIG_STATUS_$? >> /home/anon/network-traffic.log\n"
    )
    with (root / "etc/SystemServer.ini").open("a") as config:
        config.write(
            "\n[NetworkTrafficProbe]\n"
            "Executable=/bin/Shell\n"
            "Arguments=/root/network-traffic.sh\n"
            "KeepAlive=false\n"
            "User=root\n"
            "SystemModes=graphical\n"
        )
    return root, help_commands, list(commands)


def send_keys(monitor, keys):
    for key in keys:
        test_window_server.monitor_command(monitor, f"sendkey {key}")
        time.sleep(0.04)


def different_bytes(first, second):
    before = first.read_bytes()
    after = second.read_bytes()
    if len(before) != len(after):
        return 0
    return sum(a != b for a, b in zip(before, after))


def wait_for_window(monitor, background, screenshot):
    deadline = time.monotonic() + 6
    while time.monotonic() < deadline:
        test_window_server.monitor_command(monitor, f"screendump {screenshot}")
        if different_bytes(background, screenshot) >= 20_000:
            return True
        time.sleep(0.25)
    return False


def boot_and_check(args, disk, work, help_commands, behavior_commands):
    serial = work / "serial.log"
    before = work / "before.ppm"
    about = work / "about.ppm"
    settings = work / "settings.ppm"
    calendar = work / "calendar.ppm"
    editor = work / "text-editor.ppm"
    edited = work / "text-editor-typed.ppm"
    save_permission = work / "text-editor-save-permission.ppm"
    saved = work / "text-editor-saved.ppm"
    with tempfile.TemporaryDirectory(prefix="powerpc-programs-monitor-", dir="/tmp") as monitor_directory:
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
        deadline = time.monotonic() + (65 if args.quick else 120)
        desktop_ready = False
        audit_complete = False
        about_visible = False
        settings_visible = False
        calendar_visible = False
        editor_visible = False
        try:
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
                    if b"KERNEL PANIC" in output or b"VERIFICATION FAILED" in output or b"CRASH:" in output:
                        break
                    if b"Service ProgramCheck has exited" in output:
                        audit_complete = True
                    if not desktop_ready and b"Entering WindowServer main loop" in output and b"Shell(" in output:
                        test_window_server.monitor_command(monitor, f"screendump {before}")
                        desktop_ready = test_desktop.screenshot_has_desktop(before)
                    if audit_complete and desktop_ready:
                        time.sleep(2)
                        send_keys(monitor, ["shift-a", "b", "o", "u", "t", "ret"])
                        about_visible = wait_for_window(monitor, before, about)
                        send_keys(monitor, ["esc"])
                        time.sleep(0.3)
                        send_keys(monitor, ["shift-s", "e", "t", "t", "i", "n", "g", "s", "ret"])
                        settings_visible = wait_for_window(monitor, before, settings)
                        send_keys(monitor, ["alt-f4"])
                        time.sleep(0.3)
                        send_keys(monitor, ["shift-c", "a", "l", "e", "n", "d", "a", "r", "ret"])
                        calendar_visible = wait_for_window(monitor, before, calendar)
                        time.sleep(3)
                        test_window_server.monitor_command(monitor, f"screendump {calendar}")
                        send_keys(monitor, ["alt-f4"])
                        time.sleep(0.3)
                        send_keys(monitor, ["shift-t", "e", "x", "t", "shift-e", "d", "i", "t", "o", "r",
                                            "spc", "slash", "h", "o", "m", "e", "slash", "a", "n", "o", "n",
                                            "slash", "e", "d", "i", "t", "o", "r", "dot", "t", "x", "t", "ret"])
                        editor_visible = wait_for_window(monitor, before, editor)
                        time.sleep(5)
                        test_window_server.monitor_command(monitor, f"screendump {editor}")
                        send_keys(monitor, ["o", "k"])
                        time.sleep(1)
                        test_window_server.monitor_command(monitor, f"screendump {edited}")
                        send_keys(monitor, ["ctrl-s"])
                        time.sleep(2)
                        test_window_server.monitor_command(monitor, f"screendump {save_permission}")
                        send_keys(monitor, ["ret"])
                        time.sleep(3)
                        test_window_server.monitor_command(monitor, f"screendump {saved}")
                        break
                    time.sleep(0.25)
        finally:
            process.terminate()
            _, errors = process.communicate(timeout=5)

    output = serial.read_bytes() if serial.exists() else b""
    log_path = work / "program-check.log"
    debugfs = subprocess.run(
        [test_desktop.find_debugfs(), "-R", f"dump /home/anon/program-check.log {log_path}", str(disk)],
        capture_output=True, text=True,
    )
    audit = log_path.read_text(errors="replace") if log_path.exists() else ""
    network_log_path = work / "network-traffic.log"
    network_dump = subprocess.run(
        [test_desktop.find_debugfs(), "-R", f"dump /home/anon/network-traffic.log {network_log_path}", str(disk)],
        capture_output=True, text=True,
    )
    network_audit = network_log_path.read_text(errors="replace") if network_log_path.exists() else ""
    editor_file = work / "editor.txt"
    subprocess.run(
        [test_desktop.find_debugfs(), "-R", f"dump /home/anon/editor.txt {editor_file}", str(disk)],
        capture_output=True, text=True,
    )
    editor_saved = editor_file.exists() and editor_file.read_text(errors="replace") == "okEditor test\n"
    missing = []
    for name in help_commands:
        if f"HELP_RESULT_{name}_0" not in audit:
            missing.append(f"{name} --help")
    for name in behavior_commands:
        if f"COMMAND_RESULT_{name}_0" not in audit:
            missing.append(name)
    for text in (
        "PROGRAM_CHECK_COMPLETE", "powerpc", "Model: PowerPC 750", "ELF 32-bit MSB", "ELF 64-bit LSB",
        "u64=18446744073709551615", "atomic64=ok", "aligned-new=ok", "sincosf=ok", "sincos=ok",
        "js-value-tag=ok", "p256=ok", "VT8231 [PCI-to-ISA Bridge]",
    ):
        if text not in audit:
            missing.append(text)
    if args.cpu == "750cxe" and "PowerPC 750CXe v3.1b" not in audit:
        missing.append("750CXe CPU model")
    if not args.quick and "pts:" not in audit:
        missing.append("w terminal name")
    if not args.quick and "Pong from 127.0.0.1" not in audit:
        missing.append("loopback ping reply")
    for expected in ("CONFIG_STATUS_0", "PING_STATUS_0", "IFCONFIG_STATUS_0", "Pong from 10.0.2.2", "class: VirtIONetworkAdapter"):
        if expected not in network_audit:
            missing.append(expected)
    received = re.search(r"RX: (\d+) packets (\d+) bytes", network_audit)
    if received is None or not (0 < int(received.group(1)) <= 8 and 0 < int(received.group(2)) < 2048):
        missing.append("virtio-net received frame length")
    if (not audit_complete or not desktop_ready or b"About(" not in output or b"Settings(" not in output
            or b"Calendar(" not in output or b"TextEditor(" not in output
            or b"Exiting with runtime error" in output or b"KERNEL PANIC" in output
            or b"VERIFICATION FAILED" in output or not about_visible or not settings_visible
            or not calendar_visible or not editor_visible or not editor_saved or missing):
        raise SystemExit(
            f"Pegasos 2 desktop program check failed: missing {missing}, audit={audit_complete}, "
            f"desktop={desktop_ready}, about={about_visible}, settings={settings_visible}, "
            f"calendar={calendar_visible}, editor={editor_visible}, editor saved={editor_saved}, "
            f"debugfs={debugfs.returncode}, network_debugfs={network_dump.returncode}\n"
            + audit[-3000:] + "\n" + output.decode(errors="replace")[-2500:]
            + "\nNetwork audit:\n" + network_audit[-1000:]
            + errors.decode(errors="replace")[-500:]
        )
    print(f"Pegasos 2 desktop ran {len(help_commands)} command help checks and {len(behavior_commands)} commands; About, Settings, Calendar, and TextEditor opened; editor save and virtio-net ping passed")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", required=True)
    parser.add_argument("--loader", type=Path, required=True)
    parser.add_argument("--user-loader", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--cxx-compiler", required=True)
    parser.add_argument("--libc", type=Path, required=True)
    parser.add_argument("--libsystem", type=Path, required=True)
    parser.add_argument("--memory", type=int, default=512)
    parser.add_argument("--cpu", default="750cxe")
    parser.add_argument("--quick", action="store_true")
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()

    source = Path(__file__).parents[3]
    if args.output_dir:
        args.output_dir.mkdir(parents=True, exist_ok=True)
        directory_context = nullcontext(tempfile.mkdtemp(prefix="powerpc-programs-", dir=args.output_dir))
    else:
        directory_context = tempfile.TemporaryDirectory(prefix="powerpc-programs-", dir="/tmp")
    with directory_context as directory:
        work = Path(directory)
        root, help_commands, behavior_commands = prepare_root(args, source, args.build_dir, args.user_loader, work, args.quick)
        disk = test_desktop.create_disk(root, work)
        boot_and_check(args, disk, work, help_commands, behavior_commands)


if __name__ == "__main__":
    main()
