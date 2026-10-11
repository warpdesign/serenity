#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

import argparse
from pathlib import Path
import re
import socket
import struct
import subprocess
import tempfile
import time


def boot_and_capture(qemu: str, loader: Path, memory: int, cpu: str | None, terminal_marker: bytes,
                     require_hex_after: bool = False, inspect=None) -> bytes:
    with tempfile.TemporaryDirectory(prefix="serenity-powerpc-loader-qemu-") as directory:
        trace = Path(directory) / "vof.log"
        monitor = Path(directory) / "monitor.sock"
        command = [
            qemu,
            "-M", "pegasos2",
            "-m", str(memory),
            "-kernel", str(loader),
            "-append", "serenity-loader-stop-before-kernel",
            "-display", "none",
            "-serial", "none",
            "-monitor", f"unix:{monitor},server,nowait",
            "-no-reboot",
            "-d", "trace:vof_write",
            "-D", str(trace),
        ]
        if cpu:
            command.extend(("-cpu", cpu))
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        try:
            deadline = time.monotonic() + 20
            while time.monotonic() < deadline and process.poll() is None:
                output = trace.read_bytes() if trace.exists() else b""
                if terminal_marker in output:
                    suffix = output.split(terminal_marker, 1)[1]
                    if not require_hex_after or re.search(rb'vof_write[^\n]*"0x([0-9a-f]{8})', suffix):
                        break
                time.sleep(0.05)
            if inspect is not None and terminal_marker in output:
                inspect(monitor, output)
        finally:
            process.terminate()
            _, errors = process.communicate(timeout=5)
        output = trace.read_bytes() if trace.exists() else b""
        if terminal_marker not in output:
            raise SystemExit(f"PowerPC loader produced no expected result:\n{errors.decode(errors='replace')}\n{output.decode(errors='replace')}")
        return output


def read_physical_memory(monitor: Path, addresses: list[tuple[int, bytes]]):
    with socket.socket(socket.AF_UNIX) as client:
        client.settimeout(3)
        client.connect(str(monitor))

        def response():
            data = b""
            while not data.endswith(b"(qemu) "):
                data += client.recv(4096)
            return data

        response()
        for address, expected in addresses:
            client.sendall(f"xp /{len(expected)}bx 0x{address:x}\n".encode())
            output = response()
            observed = bytearray()
            for line in output.splitlines():
                if re.match(rb"^[0-9a-fA-F]{8}:", line):
                    observed.extend(int(value, 16) for value in re.findall(rb"0x([0-9a-fA-F]{2})(?![0-9a-fA-F])", line))
            if observed != expected:
                raise SystemExit(f"QEMU physical memory at {address:#x}: expected {expected.hex()}, got {observed.hex()}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", required=True)
    parser.add_argument("--loader", type=Path, required=True)
    parser.add_argument("--kernel-elf", type=Path, required=True)
    parser.add_argument("--loader-dir", type=Path, required=True)
    parser.add_argument("--cross-compile", required=True)
    parser.add_argument("--memory", type=int, default=256)
    parser.add_argument("--cpu")
    parser.add_argument("--corrupt-only", action="store_true")
    args = parser.parse_args()

    if not args.corrupt_only:
        source = args.kernel_elf.read_bytes()

        def inspect(monitor: Path, output: bytes):
            suffix = output.split(b"SerenityOS PowerPC loader: physical base", 1)[1]
            physical = re.search(rb'vof_write[^\n]*"0x([0-9a-f]{8})', suffix)
            if not physical:
                raise SystemExit("PowerPC loader did not report the claimed physical address")
            base = int(physical.group(1), 16)
            runtime_suffix = output.split(b"SerenityOS PowerPC loader: runtime base", 1)[1]
            runtime_match = re.search(rb'vof_write[^\n]*"0x([0-9a-f]{8})', runtime_suffix)
            if not runtime_match:
                raise SystemExit("PowerPC loader did not report the runtime base")
            runtime_base = int(runtime_match.group(1), 16)
            phoff = int.from_bytes(source[28:32], "big")
            phentsize = int.from_bytes(source[42:44], "big")
            phnum = int.from_bytes(source[44:46], "big")
            segments = []
            dynamic = None
            for index in range(phnum):
                header = phoff + index * phentsize
                segment_type = int.from_bytes(source[header:header + 4], "big")
                if segment_type == 2:
                    dynamic = (int.from_bytes(source[header + 4:header + 8], "big"),
                               int.from_bytes(source[header + 16:header + 20], "big"))
                if segment_type != 1:
                    continue
                file_offset = int.from_bytes(source[header + 4:header + 8], "big")
                virtual_offset = int.from_bytes(source[header + 8:header + 12], "big")
                file_size = int.from_bytes(source[header + 16:header + 20], "big")
                memory_size = int.from_bytes(source[header + 20:header + 24], "big")
                segments.append((virtual_offset, file_offset, file_size, memory_size))
            first = min(segment[0] for segment in segments)
            if runtime_base != 0xc0000000 + base - first:
                raise SystemExit("PowerPC loader selected the wrong high kernel base")
            reads = []
            for virtual_offset, file_offset, file_size, memory_size in segments:
                target = base + virtual_offset - first
                if file_size:
                    count = min(16, file_size)
                    reads.append((target, bytearray(source[file_offset:file_offset + count])))
                    reads.append((target + file_size - count, bytearray(source[file_offset + file_size - count:file_offset + file_size])))
                if memory_size > file_size:
                    count = min(16, memory_size - file_size)
                    reads.append((target + file_size, bytearray(count)))
                    reads.append((target + memory_size - count, bytearray(count)))

            if dynamic is None:
                raise SystemExit("The linked kernel has no dynamic section")
            dynamic_offset, dynamic_size = dynamic
            tags = {}
            for offset in range(dynamic_offset, dynamic_offset + dynamic_size, 8):
                tag, value = struct.unpack_from(">II", source, offset)
                if tag == 0:
                    break
                tags[tag] = value
            rela_address, relative_count = tags[7], tags[0x6FFFFFF9]
            rela_file = next(file_offset + rela_address - virtual_offset
                             for virtual_offset, file_offset, file_size, _ in segments
                             if virtual_offset <= rela_address < virtual_offset + file_size)
            selected_relocations = {0, relative_count // 2, relative_count - 1}
            for index in range(relative_count):
                patch_offset, info, addend = struct.unpack_from(">IIi", source, rela_file + index * 12)
                if info & 0xFF != 22:
                    raise SystemExit("The kernel's relative relocation prefix changed")
                patch = (runtime_base + addend).to_bytes(4, "big")
                target = base + patch_offset - first
                for address, expected in reads:
                    for byte_index in range(4):
                        position = target + byte_index - address
                        if 0 <= position < len(expected):
                            expected[position] = patch[byte_index]
                if index in selected_relocations:
                    reads.append((target, bytearray(patch)))
            read_physical_memory(monitor, reads)

        output = boot_and_capture(args.qemu, args.loader, args.memory, args.cpu,
                                  b"SerenityOS PowerPC loader: PCI bridge handoff prepared", inspect=inspect)
        for marker in (
            b"SerenityOS PowerPC loader: kernel ELF32 validated",
            b"SerenityOS PowerPC loader: load span",
            b"SerenityOS PowerPC loader: entry",
            b"SerenityOS PowerPC loader: kernel segments loaded",
            b"SerenityOS PowerPC loader: physical base",
            b"SerenityOS PowerPC loader: kernel relocations applied",
            b"SerenityOS PowerPC loader: relocation count",
            b"SerenityOS PowerPC loader: runtime base",
            b"SerenityOS PowerPC loader: kernel high mappings prepared",
            b"SerenityOS PowerPC loader: hash table physical",
            b"SerenityOS PowerPC loader: mapped pages",
            b"SerenityOS PowerPC loader: high kernel alias passed",
            b"SerenityOS PowerPC loader: boot handoff validated",
            b"SerenityOS PowerPC loader: PCI bridge handoff prepared",
        ):
            if marker not in output:
                raise SystemExit(f"PowerPC loader missed {marker!r}:\n{output.decode(errors='replace')}")
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
        suffix = output.split(b"SerenityOS PowerPC loader: physical base", 1)[1]
        physical = re.search(rb'vof_write[^\n]*"0x([0-9a-f]{8})', suffix)
        if not physical or int(physical.group(1), 16) % (2 * 1024 * 1024) != 0:
            raise SystemExit(f"PowerPC loader did not claim aligned RAM:\n{output.decode(errors='replace')}")
        count_suffix = output.split(b"SerenityOS PowerPC loader: relocation count", 1)[1]
        observed_count = re.search(rb'vof_write[^\n]*"0x([0-9a-f]{8})', count_suffix)
        dynamic_header = next(phoff + index * phentsize for index in range(phnum)
                              if int.from_bytes(source[phoff + index * phentsize:phoff + index * phentsize + 4], "big") == 2)
        dynamic_offset = int.from_bytes(source[dynamic_header + 4:dynamic_header + 8], "big")
        dynamic_size = int.from_bytes(source[dynamic_header + 16:dynamic_header + 20], "big")
        tags = dict(struct.iter_unpack(">II", source[dynamic_offset:dynamic_offset + dynamic_size]))
        if not observed_count or int(observed_count.group(1), 16) != tags[0x6FFFFFF9]:
            raise SystemExit("PowerPC loader reported the wrong relocation count")
        table_suffix = output.split(b"SerenityOS PowerPC loader: hash table physical", 1)[1]
        table_address = re.search(rb'vof_write[^\n]*"0x([0-9a-f]{8})', table_suffix)
        if not table_address or int(table_address.group(1), 16) % (256 * 1024) != 0:
            raise SystemExit("PowerPC loader did not align its hash table")
        symbols = subprocess.check_output([args.cross_compile + "nm", "--defined-only", str(args.kernel_elf)], text=True)
        image_end = next(int(line.split()[0], 16) for line in symbols.splitlines()
                         if line.endswith(" end_of_kernel_image"))
        if int(table_address.group(1), 16) < int(physical.group(1), 16) + image_end - first:
            raise SystemExit("PowerPC hash table overlaps the linked kernel image or initial stack")
        page_suffix = output.split(b"SerenityOS PowerPC loader: mapped pages", 1)[1]
        page_count = re.search(rb'vof_write[^\n]*"0x([0-9a-f]{8})', page_suffix)
        expected_pages = sum((int.from_bytes(source[header + 20:header + 24], "big") + 4095) // 4096
                             for header in load_headers)
        if not page_count or int(page_count.group(1), 16) != expected_pages:
            raise SystemExit("PowerPC loader mapped the wrong number of kernel pages")
        print("Pegasos 2 kernel ELF loader probe passed")
        return

    with tempfile.TemporaryDirectory(prefix="serenity-powerpc-loader-invalid-") as directory:
        work = Path(directory)

        def check_rejected(name: str, image: bytearray, expected_marker: bytes):
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
            output = boot_and_capture(args.qemu, bad_loader, args.memory, args.cpu, expected_marker)
            if expected_marker not in output or b"kernel relocations applied" in output:
                raise SystemExit(f"PowerPC loader accepted {name}:\n{output.decode(errors='replace')}")

        source = args.kernel_elf.read_bytes()
        wrong_endian = bytearray(source)
        wrong_endian[5] = 1  # EI_DATA: little-endian kernel on big-endian PPC.
        check_rejected("wrong-endian", wrong_endian, b"SerenityOS PowerPC loader: kernel ELF invalid")

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
        check_rejected("overlapping-loads", overlapping, b"SerenityOS PowerPC loader: kernel ELF invalid")

        bad_relocations = bytearray(source)
        dynamic_header = next(phoff + index * phentsize for index in range(phnum)
                              if int.from_bytes(source[phoff + index * phentsize:phoff + index * phentsize + 4], "big") == 2)
        dynamic_offset = int.from_bytes(source[dynamic_header + 4:dynamic_header + 8], "big")
        dynamic_size = int.from_bytes(source[dynamic_header + 16:dynamic_header + 20], "big")
        for offset in range(dynamic_offset, dynamic_offset + dynamic_size, 8):
            if int.from_bytes(source[offset:offset + 4], "big") == 7:
                bad_relocations[offset + 4:offset + 8] = (0xFFFFFF00).to_bytes(4, "big")
                break
        check_rejected("invalid-rela-address", bad_relocations, b"SerenityOS PowerPC loader: kernel relocation failed")

        tags = dict(struct.iter_unpack(">II", source[dynamic_offset:dynamic_offset + dynamic_size]))
        rela_address = tags[7]
        rela_file = next(
            int.from_bytes(source[header + 4:header + 8], "big")
            + rela_address - int.from_bytes(source[header + 8:header + 12], "big")
            for header in load_headers
            if int.from_bytes(source[header + 8:header + 12], "big") <= rela_address
            < int.from_bytes(source[header + 8:header + 12], "big")
            + int.from_bytes(source[header + 16:header + 20], "big")
        )
        bad_target = bytearray(source)
        bad_target[rela_file:rela_file + 4] = (0xFFFFFFFC).to_bytes(4, "big")
        check_rejected("invalid-relocation-target", bad_target, b"SerenityOS PowerPC loader: kernel relocation failed")
    print("Pegasos 2 corrupt kernel ELF and relocation layouts rejected")


if __name__ == "__main__":
    main()
