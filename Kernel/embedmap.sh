#!/bin/sh
# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

set -e

tmp=$(mktemp)
reserved=$(mktemp)
trap 'rm -f "$tmp" "$reserved"' EXIT
OBJCOPY="${OBJCOPY:-objcopy}"
"$OBJCOPY" --dump-section .ksyms="$reserved" Kernel
reserved_size=$(wc -c < "$reserved")
(cat kernel.map; printf '%b' '\0') > "$tmp"
python3 - "$tmp" "$reserved_size" <<'PY'
import sys

with open(sys.argv[1], "r+b") as section:
    reserved_size = int(sys.argv[2])
    if section.seek(0, 2) > reserved_size:
        raise SystemExit("kernel.map exceeds the reserved .ksyms section")
    section.truncate(reserved_size)
PY
"$OBJCOPY" --update-section .ksyms="$tmp" Kernel
