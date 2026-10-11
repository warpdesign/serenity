/*
 * Copyright (c) 2023-2026, Sönke Holz <soenke.holz@serenityos.org>
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

ENTRY(init)
OUTPUT_FORMAT(elf32-powerpc)

ASSERT(CONSTANT(MAXPAGESIZE) == 4K, "The PowerPC kernel linker script requires 4K pages");

#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

PHDRS
{
    text PT_LOAD FLAGS(PF_R | PF_X);
    data PT_LOAD FLAGS(PF_R | PF_W);
    bss PT_LOAD FLAGS(PF_R | PF_W);
    dynamic_segment PT_LOAD FLAGS(PF_R | PF_W);
    dynamic PT_DYNAMIC FLAGS(PF_R | PF_W);
    ksyms PT_LOAD FLAGS(PF_R | PF_W);
}

SECTIONS
{
    start_of_kernel_image = .;

    .text ALIGN(4K) :
    {
        start_of_kernel_text = .;
        *(.text.first)

        start_of_safemem_text = .;
        KEEP(*(.text.safemem))
        end_of_safemem_text = .;
        start_of_safemem_atomic_text = .;
        KEEP(*(.text.safemem.atomic))
        end_of_safemem_atomic_text = .;

        *(.text* .glink* .plt*)
    } :text

    .driver_init ALIGN(4K) :
    {
        driver_init_table_start = .;
        *(.driver_init)
        driver_init_table_end = .;
    } :text

    .unmap_after_init ALIGN(4K) :
    {
        start_of_unmap_after_init = .;
        *(.unmap_after_init*)
        end_of_unmap_after_init = .;
        end_of_kernel_text = .;
    } :text

    .rodata ALIGN(4K) :
    {
        start_ctors = .;
        *(.init_array)
        end_ctors = .;
        *(.srodata* .rodata* .rela.dyn)
    } :data

    .data ALIGN(4K) :
    {
        start_of_kernel_data = .;
        *(.sdata* .data* .got*)
        end_of_kernel_data = .;
    } :data

    .ro_after_init ALIGN(4K) :
    {
        start_of_ro_after_init = .;
        *(.ro_after_init)
        end_of_ro_after_init = .;
    } :data

    .bss ALIGN(4K) (NOLOAD) :
    {
        start_of_bss = .;
        *(.sbss* .bss* COMMON)
        end_of_bss = .;

        . = ALIGN(4K);
        *(.heap)
    } :bss

    .dynamic ALIGN(4K) :
    {
        *(.dynamic)
    } :dynamic_segment :dynamic

    .ksyms ALIGN(4K) :
    {
        start_of_kernel_ksyms = .;
        *(.kernel_symbols)
        end_of_kernel_ksyms = .;
    } :ksyms

    .initial_stack ALIGN(4K) (NOLOAD) :
    {
        start_of_initial_stack = .;
        . += 32K;
        end_of_initial_stack = .;
    } :ksyms

    end_of_kernel_image = .;
}
