/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);
    image PT_LOAD FLAGS(4);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0x00200000;
    .text : { KEEP(*(.text.first)) *(.text*) } :text
    .kernel_image ALIGN(4096) : { KEEP(*(.rodata.kernel)) } :image
    .rodata ALIGN(4096) : { *(.rodata*) *(.eh_frame*) } :data
    .data : { *(.data*) *(.sdata*) } :data
    .bss (NOLOAD) :
    {
        __bss_start = .;
        *(.bss*) *(.sbss*) *(COMMON)
        . = ALIGN(16);
        __bss_end = .;
        . += 32K;
        __stack_end = .;
    } :data
}
