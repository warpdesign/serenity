#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--qemu", required=True)
    parser.add_argument("--kernel-loader", required=True)
    parser.add_argument("--user-loader", required=True)
    parser.add_argument("--library-root", type=Path, required=True)
    parser.add_argument("--memory", type=int, default=512)
    parser.add_argument("--cpu")
    args = parser.parse_args()

    mke2fs = shutil.which("mke2fs")
    if mke2fs is None:
        homebrew_mke2fs = Path("/opt/homebrew/opt/e2fsprogs/sbin/mke2fs")
        if homebrew_mke2fs.exists():
            mke2fs = str(homebrew_mke2fs)
    if mke2fs is None:
        raise SystemExit("mke2fs is required for the PowerPC PNG runtime test")

    source_root = Path(__file__).parents[3]
    libraries = {path.name: path for path in args.library_root.glob("**/*.so*") if path.is_file()}
    library_directories = sorted({path.parent for path in libraries.values()})

    with tempfile.TemporaryDirectory(prefix="serenity-powerpc-png-") as directory:
        work = Path(directory)
        executable = work / "init"
        compile_command = [
            args.compiler, "-m32", "-msecure-plt", "-fPIC", "-fno-stack-protector",
            "-fno-exceptions", "-fno-rtti", "-O2", "-std=c++23", "-nostartfiles",
            "-nodefaultlibs", "-Wl,-e,_start", "-Wl,--dynamic-linker=/usr/lib/Loader.so",
            "-I" + str(source_root), "-I" + str(source_root / "Userland/Libraries"),
        ]
        for library_directory in library_directories:
            compile_command.extend(("-L" + str(library_directory), "-Wl,-rpath-link," + str(library_directory)))
        compile_command.extend((
            str(Path(__file__).with_name("PNGProbe.cpp")), "-lgfx", "-lcompress", "-lc", "-lsystem", "-lgcc", "-o", str(executable),
        ))
        subprocess.run(compile_command, check=True)

        root = work / "root"
        (root / "usr/lib").mkdir(parents=True)
        shutil.copy2(executable, root / "init")
        shutil.copy2(args.user_loader, root / "usr/lib/Loader.so")
        for name, library in libraries.items():
            shutil.copy2(library, root / "usr/lib" / name)
        shutil.copy2(source_root / "Base/res/cursor-themes/Default/arrow.x2y2.png", root / "cursor.png")

        disk = work / "root.ext2"
        with disk.open("wb") as file:
            file.truncate(128 * 1024 * 1024)
        subprocess.run([mke2fs, "-q", "-F", "-t", "ext2", "-O", "none", "-b", "1024", "-d", str(root), str(disk)], check=True)
        with disk.open("r+b") as file:
            file.seek(510)
            file.write(b"\x55\xaa")

        serial = work / "serial.log"
        command = [
            args.qemu, "-M", "pegasos2", "-m", str(args.memory), "-kernel", args.kernel_loader,
            "-append", "root=lun3:0:0", "-drive", f"if=none,format=raw,id=virtio_disk,file={disk}",
            "-device", "virtio-blk-pci,drive=virtio_disk", "-object", "rng-random,id=rng0,filename=/dev/urandom",
            "-device", "virtio-rng-pci,rng=rng0", "-display", "none", "-serial", f"file:{serial}",
            "-monitor", "none", "-no-reboot", "-icount", "shift=auto",
        ]
        if args.cpu:
            command.extend(("-cpu", args.cpu))
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        passed = False
        try:
            deadline = time.monotonic() + 175
            while time.monotonic() < deadline and process.poll() is None:
                output = serial.read_bytes() if serial.exists() else b""
                if b"PowerPC PNG cursor decode and writer passed" in output:
                    passed = True
                    break
                if b"PowerPC PNG cursor" in output or b"KERNEL PANIC" in output or b"CRASH:" in output:
                    break
                time.sleep(0.05)
        finally:
            process.terminate()
            _, errors = process.communicate(timeout=5)

        output = serial.read_bytes() if serial.exists() else b""
        if not passed or b"KERNEL PANIC" in output or b"CRASH:" in output:
            raise SystemExit("Pegasos 2 PNG cursor decode failed:\n" + output.decode(errors="replace")[-3000:] + errors.decode(errors="replace")[-1000:])
    print("Pegasos 2 PNG cursor decode and writer byte order passed")


if __name__ == "__main__":
    main()
