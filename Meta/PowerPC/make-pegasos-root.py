#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

"""Build the ext2 desktop root for a Pegasos 2 SD card, using the PowerPC desktop test recipe.

    make-pegasos-root.py --build-dir Build/powerpc --output root.ext2 [--boot-benchmark]
    make-pegasos-card.py card --loader serenity.elf --root root.ext2 --output card.img

--boot-benchmark adds a root service that waits for the desktop to settle, then runs
membench on the RAM, the Radeon framebuffer and the raw IDE disk. Its results go to the
serial console (through the kernel log) and to /home/anon/membench.log.
"""

import argparse
import shutil
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
SOURCE = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(SOURCE / "Tests/Kernel/PowerPC"))
import test_desktop  # noqa: E402


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--boot-benchmark", action="store_true")
    parser.add_argument("--benchmark-delay", type=int, default=60, help="seconds to wait after boot (default 60)")
    args = parser.parse_args()

    build = args.build_dir.resolve()
    with tempfile.TemporaryDirectory(prefix="pegasos-root-") as directory:
        work = Path(directory)
        root = test_desktop.create_root(SOURCE, build, build / "Userland/DynamicLoader/Loader.so", work)
        if args.boot_benchmark:
            (root / "root/boot-benchmark.sh").write_text(
                f"sleep {args.benchmark_delay}\n"
                "/bin/membench -f /dev/gpu/connector0 -d /dev/hda > /home/anon/membench.log 2>&1\n"
                "echo MEMBENCH_STATUS_$? >> /home/anon/membench.log\n"
            )
            with (root / "etc/SystemServer.ini").open("a") as config:
                config.write(
                    "\n[BootBenchmark]\n"
                    "Executable=/bin/Shell\n"
                    "Arguments=/root/boot-benchmark.sh\n"
                    "KeepAlive=false\n"
                    "User=root\n"
                    "SystemModes=graphical\n"
                )
        disk = test_desktop.create_disk(root, work)
        shutil.move(disk, args.output)
    print(f"{args.output}: {args.output.stat().st_size // 1048576} MiB ext2 root")


if __name__ == "__main__":
    main()
