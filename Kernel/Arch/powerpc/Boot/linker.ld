/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

ENTRY(_start)

SECTIONS
{
    . = 0x00200000;
    .text :
    {
        KEEP(*(.text.first))
        start_of_safemem_text = .;
        KEEP(*(.text.safemem))
        end_of_safemem_text = .;
        start_of_safemem_atomic_text = .;
        KEEP(*(.text.safemem.atomic))
        end_of_safemem_atomic_text = .;
        *(.text*)
    }
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
