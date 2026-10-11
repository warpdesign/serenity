/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

PHDRS { text PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); }
SECTIONS {
    . = 0;
    .text : { *(.text.start) . = ALIGN(4096); *(.text.second) *(.text*) } :text
    . = ALIGN(4096);
    .bss (NOLOAD) : { *(.bss*) } :data
    /DISCARD/ : { *(.dynamic) *(.dynsym) *(.dynstr) *(.hash) *(.gnu.hash) *(.got) *(.got.plt) }
}
