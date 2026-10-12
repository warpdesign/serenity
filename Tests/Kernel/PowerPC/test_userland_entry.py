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
            raise RuntimeError("QEMU monitor closed while reading registers")
        output += chunk
    return output.decode(errors="replace")


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
        raise SystemExit("mke2fs is required for the PowerPC user entry test")

    fixtures = Path(__file__).parent / "Fixtures"
    syscall_header = Path(__file__).parents[3] / "Kernel/API/Syscall.h"
    syscall_names = re.findall(r"^\s*S\((\w+),", syscall_header.read_text(), re.MULTILINE)
    getpid_number = syscall_names.index("getpid")
    getcwd_number = syscall_names.index("getcwd")
    dbgputstr_number = syscall_names.index("dbgputstr")
    with tempfile.TemporaryDirectory(prefix="serenity-powerpc-user-entry-") as directory:
        work = Path(directory)
        object_file = work / "user-loop.o"
        executable = work / "init"
        subprocess.run([
            args.cross_compile + "gcc", "-m32", "-msdata=none", "-ffreestanding", "-fno-pic", "-fno-pie",
            f"-DSYSCALL_GETPID={getpid_number}",
            f"-DSYSCALL_GETCWD={getcwd_number}",
            f"-DSYSCALL_DBGPUTSTR={dbgputstr_number}",
            "-c", str(fixtures / "UserLoop.S"), "-o", str(object_file),
        ], check=True)
        subprocess.run([
            args.cross_compile + "ld", "-m", "elf32ppcserenity", "-shared", "--no-dynamic-linker",
            "-T", str(fixtures / "UserLoop.ld"), "--entry=_start", str(object_file), "-o", str(executable),
        ], check=True)

        root = work / "root"
        root.mkdir()
        shutil.copyfile(executable, root / "init")
        disk = work / "root.ext2"
        with disk.open("wb") as file:
            file.truncate(16 * 1024 * 1024)
        subprocess.run([mke2fs, "-q", "-F", "-t", "ext2", "-O", "none", "-b", "1024", "-d", str(root), str(disk)], check=True)
        with disk.open("r+b") as file:
            file.seek(510)
            file.write(b"\x55\xaa")

        serial = work / "serial.log"
        interrupts = work / "interrupts.log"
        monitor_path = work / "monitor.sock"
        command = [
            args.qemu, "-M", "pegasos2", "-m", str(args.memory), "-kernel", args.loader,
            "-append", "root=lun3:0:0",
            "-drive", f"if=none,format=raw,id=virtio_disk,file={disk}",
            "-device", "virtio-blk-pci,drive=virtio_disk",
            "-device", "virtio-keyboard-pci",
            "-object", "rng-random,id=rng0,filename=/dev/urandom",
            "-device", "virtio-rng-pci,rng=rng0",
            "-display", "none", "-serial", f"file:{serial}",
            "-monitor", f"unix:{monitor_path},server=on,wait=off",
            "-no-reboot", "-icount", "shift=auto",
            "-d", "int", "-D", str(interrupts),
        ]
        if args.cpu:
            command.extend(("-cpu", args.cpu))
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        observed_user_loop = 0
        trace_position = 0
        try:
            deadline = time.monotonic() + 12
            while not monitor_path.exists() and time.monotonic() < deadline and process.poll() is None:
                time.sleep(0.05)
            with socket.socket(socket.AF_UNIX) as monitor:
                monitor.settimeout(3)
                monitor.connect(str(monitor_path))
                while time.monotonic() < deadline and process.poll() is None:
                    registers = read_monitor(monitor, "info registers")
                    nip_match = re.search(r"NIP ([0-9a-fA-F]{8})", registers)
                    msr_match = re.search(r"MSR ([0-9a-fA-F]{8})", registers)
                    if nip_match and msr_match and int(msr_match.group(1), 16) & 0x4000:
                        nip = int(nip_match.group(1), 16)
                        instruction = read_monitor(monitor, f"x /1wx 0x{nip:x}")
                        if re.search(r"0x48000000", instruction):
                            observed_user_loop += 1
                            if observed_user_loop == 2:
                                break
                            read_monitor(monitor, "sendkey a")
                            time.sleep(0.5)
                            continue
                    if serial.exists() and b"KERNEL PANIC" in serial.read_bytes():
                        break
                    if interrupts.exists():
                        with interrupts.open("rb") as trace:
                            trace.seek(trace_position)
                            new_trace = trace.read()
                            trace_position = trace.tell()
                        if b"HV_EMU" in new_trace:
                            break
                    time.sleep(0.05)
        finally:
            process.terminate()
            _, errors = process.communicate(timeout=5)

        serial_output = serial.read_bytes() if serial.exists() else b""
        interrupt_lines = interrupts.read_text(errors="replace").splitlines() if interrupts.exists() else []
        user_instruction_faults = sum("=> ISI" in line and int(line.split(" at ")[1].split(" =>")[0], 16) < 0xc0000000 for line in interrupt_lines if " at " in line)
        user_faults = sum("=> DSI" in line and int(line.split(" at ")[1].split(" =>")[0], 16) < 0xc0000000 for line in interrupt_lines if " at " in line)
        user_timer_interrupts = sum("=> DECR" in line and int(line.split(" at ")[1].split(" =>")[0], 16) < 0xc0000000 for line in interrupt_lines if " at " in line)
        user_external_interrupts = sum("=> EXTERNAL" in line and int(line.split(" at ")[1].split(" =>")[0], 16) < 0xc0000000 for line in interrupt_lines if " at " in line)
        user_syscalls = sum("=> SYSCALL" in line and int(line.split(" at ")[1].split(" =>")[0], 16) < 0xc0000000 for line in interrupt_lines if " at " in line)
        if observed_user_loop != 2 or user_instruction_faults < 1 or user_faults < 1 or user_timer_interrupts < 2 or user_external_interrupts < 1 or user_syscalls < 3 or b"PPC user syscall" not in serial_output or b"KERNEL PANIC" in serial_output:
            raise SystemExit(
                f"Pegasos 2 user entry failed: loop samples={observed_user_loop}, user instruction faults={user_instruction_faults}, user data faults={user_faults}, user timer interrupts={user_timer_interrupts}, user external interrupts={user_external_interrupts}, user syscalls={user_syscalls}\n"
                + serial_output.decode(errors="replace")[-3000:]
                + errors.decode(errors="replace")[-1000:]
            )
    print("Pegasos 2 user entry, faults, timer/external interrupts, and syscall passed")


if __name__ == "__main__":
    main()
