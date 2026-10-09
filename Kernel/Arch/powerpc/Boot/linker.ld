/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

ENTRY(_start)

SECTIONS
{
    . = 0x00200000;
    .text : { KEEP(*(.text.first)) *(.text*) }
    __text_end = .;
    .rodata : { *(.rodata*) }
    .data : { *(.data*) *(.sdata*) }
    .bss (NOLOAD) :
    {
        __bss_start = .;
        *(.bss*) *(.sbss*) *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
        . = ALIGN(16);
        . += 32K;
        __stack_end = .;
    }
}
