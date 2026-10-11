#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

import argparse
from pathlib import Path
import re
import subprocess
import tempfile
import time


def boot_and_capture(qemu: str, loader: Path) -> bytes:
    with tempfile.TemporaryDirectory(prefix="serenity-powerpc-loader-qemu-") as directory:
        trace = Path(directory) / "vof.log"
        command = [
            qemu,
            "-M", "pegasos2",
            "-m", "256",
            "-kernel", str(loader),
            "-display", "none",
            "-serial", "none",
            "-monitor", "none",
            "-no-reboot",
            "-d", "trace:vof_write",
            "-D", str(trace),
        ]
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        try:
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline and process.poll() is None:
                output = trace.read_bytes() if trace.exists() else b""
                if b"SerenityOS PowerPC loader: kernel ELF" in output:
                    break
                time.sleep(0.05)
        finally:
            process.terminate()
            _, errors = process.communicate(timeout=5)
        output = trace.read_bytes() if trace.exists() else b""
        if b"SerenityOS PowerPC loader: kernel ELF" not in output:
            raise SystemExit(f"PowerPC loader produced no ELF result:\n{errors.decode(errors='replace')}\n{output.decode(errors='replace')}")
        return output


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", required=True)
    parser.add_argument("--loader", type=Path, required=True)
    parser.add_argument("--kernel-elf", type=Path, required=True)
    parser.add_argument("--loader-dir", type=Path, required=True)
    parser.add_argument("--cross-compile", required=True)
    parser.add_argument("--corrupt-only", action="store_true")
    args = parser.parse_args()

    if not args.corrupt_only:
        output = boot_and_capture(args.qemu, args.loader)
        for marker in (
            b"SerenityOS PowerPC loader: kernel ELF32 validated",
            b"SerenityOS PowerPC loader: load span",
            b"SerenityOS PowerPC loader: entry",
        ):
            if marker not in output:
                raise SystemExit(f"PowerPC loader missed {marker!r}:\n{output.decode(errors='replace')}")
        source = args.kernel_elf.read_bytes()
        phoff = int.from_bytes(source[28:32], "big")
        phentsize = int.from_bytes(source[42:44], "big")
        phnum = int.from_bytes(source[44:46], "big")
        load_headers = [phoff + index * phentsize for index in range(phnum)
                        if int.from_bytes(source[phoff + index * phentsize:phoff + index * phentsize + 4], "big") == 1]
        first = min(int.from_bytes(source[header + 8:header + 12], "big") for header in load_headers)
        end = max(int.from_bytes(source[header + 8:header + 12], "big")
                  + int.from_bytes(source[header + 20:header + 24], "big") for header in load_headers)
        expected_span = ((end + 4095) & ~4095) - first
        expected_entry = int.from_bytes(source[24:28], "big")
        for label, value in ((b"load span", expected_span), (b"entry", expected_entry)):
            suffix = output.split(b"SerenityOS PowerPC loader: " + label, 1)[1]
            observed = re.search(rb'vof_write[^\n]*"0x([0-9a-f]{8})', suffix)
            if not observed or int(observed.group(1), 16) != value:
                raise SystemExit(f"PowerPC loader reported the wrong {label.decode()}:\n{output.decode(errors='replace')}")
        print("Pegasos 2 kernel ELF loader probe passed")
        return

    with tempfile.TemporaryDirectory(prefix="serenity-powerpc-loader-invalid-") as directory:
        work = Path(directory)

        def check_rejected(name: str, image: bytearray):
            bad_kernel = work / f"{name}.elf"
            bad_kernel.write_bytes(image)
            bad_loader = work / name / "serenity-powerpc-loader.elf"
            command = [
                "make", "-C", str(args.loader_dir), "all",
                f"BUILD_DIR={bad_loader.parent}",
                f"KERNEL_ELF={bad_kernel}",
                f"CROSS_COMPILE={args.cross_compile}",
            ]
            subprocess.run(command, check=True, capture_output=True)
            output = boot_and_capture(args.qemu, bad_loader)
            if b"SerenityOS PowerPC loader: kernel ELF invalid" not in output or b"kernel ELF32 validated" in output:
                raise SystemExit(f"PowerPC loader accepted {name}:\n{output.decode(errors='replace')}")

        source = args.kernel_elf.read_bytes()
        wrong_endian = bytearray(source)
        wrong_endian[5] = 1  # EI_DATA: little-endian kernel on big-endian PPC.
        check_rejected("wrong-endian", wrong_endian)

        overlapping = bytearray(source)
        phoff = int.from_bytes(source[28:32], "big")
        phentsize = int.from_bytes(source[42:44], "big")
        phnum = int.from_bytes(source[44:46], "big")
        load_headers = [phoff + index * phentsize for index in range(phnum)
                        if int.from_bytes(source[phoff + index * phentsize:phoff + index * phentsize + 4], "big") == 1]
        if len(load_headers) < 2:
            raise SystemExit("The linked kernel has fewer than two loadable segments")
        second = load_headers[1]
        overlapping[second + 8:second + 16] = source[load_headers[0] + 8:load_headers[0] + 16]
        check_rejected("overlapping-loads", overlapping)
    print("Pegasos 2 corrupt kernel ELF layouts rejected")


if __name__ == "__main__":
    main()
