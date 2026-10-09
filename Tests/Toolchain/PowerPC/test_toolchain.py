#!/usr/bin/env python3
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

import argparse
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--readelf", required=True)
    args = parser.parse_args()

    target = subprocess.check_output([args.compiler, "-dumpmachine"], text=True).strip()
    if not target.startswith("powerpc-") or not target.endswith("-serenity"):
        raise SystemExit(f"Unexpected compiler target: {target}")

    macros = subprocess.check_output([args.compiler, "-dM", "-E", "-x", "c", "/dev/null"], text=True)
    for definition in ("#define __serenity__ 1", "#define __SIZEOF_POINTER__ 4", "#define __BYTE_ORDER__ __ORDER_BIG_ENDIAN__"):
        if definition not in macros.splitlines():
            raise SystemExit(f"Missing target macro: {definition}")
    if not any(definition in macros.splitlines() for definition in ("#define __powerpc__ 1", "#define _ARCH_PPC 1")):
        raise SystemExit("Missing PowerPC architecture macro")

    multilibs = subprocess.check_output([args.compiler, "-print-multi-lib"], text=True).strip().splitlines()
    if multilibs not in ([".;"], ["."]):
        raise SystemExit(f"Unexpected PowerPC multilib variants: {multilibs}")

    with tempfile.TemporaryDirectory(prefix="serenity-powerpc-toolchain-") as directory:
        source = Path(directory) / "abi.c"
        object_file = Path(directory) / "abi.o"
        source.write_text("_Static_assert(sizeof(void*) == 4, \"PowerPC pointer width\");\nint abi_probe(void) { return sizeof(void*); }\n")
        subprocess.run([args.compiler, "-ffreestanding", "-fno-pic", "-fno-pie", "-c", str(source), "-o", str(object_file)], check=True)
        header = subprocess.check_output([args.readelf, "-h", str(object_file)], text=True)
        link_plan = subprocess.run([args.compiler, "-###", str(source), "-o", str(Path(directory) / "abi")], capture_output=True, text=True, check=True).stderr

        plt_source = Path(directory) / "plt.c"
        plt_image = Path(directory) / "plt.so"
        crtbegin = subprocess.check_output([args.compiler, "-print-file-name=crtbeginS.o"], text=True).strip()
        crtend = subprocess.check_output([args.compiler, "-print-file-name=crtendS.o"], text=True).strip()
        plt_source.write_text("extern int imported_function(int);\nint call_imported_function(int value) { return imported_function(value); }\n")
        subprocess.run([args.compiler, "-msdata=none", "-fPIC", "-shared", "-nostdlib", crtbegin, str(plt_source), crtend, "-o", str(plt_image)], check=True)
        plt_sections = subprocess.check_output([args.readelf, "-W", "-S", str(plt_image)], text=True)
        plt_dynamic = subprocess.check_output([args.readelf, "-W", "-d", "-r", str(plt_image)], text=True)

        resolver_source = Path(directory) / "resolver.c"
        trampoline_image = Path(directory) / "trampoline.so"
        trampoline_source = Path(__file__).resolve().parents[3] / "Userland/Libraries/LibELF/Arch/powerpc/plt_trampoline.S"
        resolver_source.write_text("unsigned _fixup_plt_entry(void* object, unsigned offset) { return (unsigned long)object + offset; }\n")
        subprocess.run([args.compiler, "-msdata=none", "-fPIC", "-shared", "-nostdlib", crtbegin, str(resolver_source), str(trampoline_source), crtend, "-o", str(trampoline_image)], check=True)
        trampoline_dynamic = subprocess.check_output([args.readelf, "-W", "-d", "-r", str(trampoline_image)], text=True)

    for field in ("ELF32", "big endian", "PowerPC"):
        if field not in header:
            raise SystemExit(f"Compiled object is missing {field}: {header}")
    if "-dynamic-linker /usr/lib/Loader.so" not in link_plan:
        raise SystemExit(f"Wrong default runtime loader: {link_plan}")
    if not any(".plt" in line and "PROGBITS" in line for line in plt_sections.splitlines()):
        raise SystemExit(f"PowerPC secure PLT must contain pointer slots: {plt_sections}")
    if not any(re.search(r"\]\s+\.got\s+PROGBITS\s", line) and re.search(r"\sWA\s+\d+\s+\d+\s+\d+\s*$", line) for line in plt_sections.splitlines()):
        raise SystemExit(f"PowerPC secure PLT must have a data GOT: {plt_sections}")
    if "PPC_GOT" not in plt_dynamic or "R_PPC_JMP_SLOT" not in plt_dynamic:
        raise SystemExit(f"PowerPC secure PLT metadata is missing: {plt_dynamic}")
    if "TEXTREL" in trampoline_dynamic or "R_PPC_REL24" in trampoline_dynamic:
        raise SystemExit(f"PowerPC PLT trampoline needs a text relocation: {trampoline_dynamic}")
    print("powerpc-serenity toolchain passed")


if __name__ == "__main__":
    main()
