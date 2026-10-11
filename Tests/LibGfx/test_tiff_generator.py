#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    generator = Path(__file__).parents[2] / "Userland/Libraries/LibGfx/TIFFGenerator.py"
    with tempfile.TemporaryDirectory(prefix="serenity-tiff-generator-") as directory:
        output = Path(directory) / "new-build" / "ImageFormats"
        subprocess.run([sys.executable, str(generator), "-o", str(output)], check=True)
        metadata = (output / "TIFFMetadata.h").read_text()
        handlers = (output / "TIFFTagHandler.cpp").read_text()
        if "enum class Type" not in metadata or "namespace Gfx::TIFF" not in handlers:
            raise SystemExit("TIFF generator did not produce usable source files")
    print("LibGfx TIFF generator fresh output directory passed")


if __name__ == "__main__":
    main()
