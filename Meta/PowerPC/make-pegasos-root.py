#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

"""Build the ext2 desktop root for a Pegasos 2 SD card, using the PowerPC desktop test recipe.

    make-pegasos-root.py --build-dir Build/powerpc --output root.ext2
    make-pegasos-card.py card --loader serenity.elf --root root.ext2 --output card.img

Run membench from Terminal after the desktop is ready.
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
    args = parser.parse_args()

    build = args.build_dir.resolve()
    with tempfile.TemporaryDirectory(prefix="pegasos-root-") as directory:
        work = Path(directory)
        root = test_desktop.create_root(SOURCE, build, build / "Userland/DynamicLoader/Loader.so", work)
        disk = test_desktop.create_disk(root, work)
        shutil.move(disk, args.output)
    print(f"{args.output}: {args.output.stat().st_size // 1048576} MiB ext2 root")


if __name__ == "__main__":
    main()
