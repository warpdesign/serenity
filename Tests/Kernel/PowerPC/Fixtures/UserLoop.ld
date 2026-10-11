/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

PHDRS { text PT_LOAD FLAGS(5); }
SECTIONS {
    . = 0;
    .text : { *(.text.start) . = ALIGN(4096); *(.text.second) *(.text*) } :text
    /DISCARD/ : { *(.dynamic) *(.dynsym) *(.dynstr) *(.hash) *(.gnu.hash) *(.got) *(.got.plt) }
}
