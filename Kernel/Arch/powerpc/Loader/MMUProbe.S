/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

.section .text, "ax"
.globl powerpc_loader_enable_translation
.type powerpc_loader_enable_translation,@function
powerpc_loader_enable_translation:
    // r3 is the aligned HPT physical base with the SDR1 size mask.
    li 4,0
    sync
    mtspr 528,4 // IBAT0U
    mtspr 530,4 // IBAT1U
    mtspr 532,4 // IBAT2U
    mtspr 534,4 // IBAT3U
    mtspr 536,4 // DBAT0U
    mtspr 538,4 // DBAT1U
    mtspr 540,4 // DBAT2U
    mtspr 542,4 // DBAT3U
    isync

    li 4,0x12 // Coherent RAM, read/write.
    mtspr 529,4 // IBAT0L
    mtspr 537,4 // DBAT0L
    li 4,0x1ffe // 256 MiB identity mapping for loader code, stack, RAM.
    mtspr 528,4
    mtspr 536,4

    lis 4,0xfe00
    ori 4,4,0x2a // Cache-inhibited, guarded PCI I/O window.
    mtspr 541,4 // DBAT2L
    lis 4,0xfe00
    ori 4,4,0x1fe // 16 MiB PCI I/O identity map for early kernel debug.
    mtspr 540,4 // DBAT2U

    lis 4,0x2000
    ori 4,4,12
    mtsr 12,4
    addi 4,4,1
    mtsr 13,4
    addi 4,4,1
    mtsr 14,4
    addi 4,4,1
    mtsr 15,4
    sync
    mtspr 25,3
    isync

    mfmsr 4
    ori 4,4,0x30
    sync
    mtmsr 4
    isync
    blr
.size powerpc_loader_enable_translation,.-powerpc_loader_enable_translation

.globl powerpc_loader_disable_translation
.type powerpc_loader_disable_translation,@function
powerpc_loader_disable_translation:
    mfmsr 3
    li 4,-49
    and 3,3,4
    sync
    mtmsr 3
    isync
    blr
.size powerpc_loader_disable_translation,.-powerpc_loader_disable_translation

.section .text.loader_marker, "ax"
.p2align 12
.globl powerpc_loader_high_marker
.type powerpc_loader_high_marker,@function
powerpc_loader_high_marker:
    li 3,0x5a5a
    blr
.size powerpc_loader_high_marker,.-powerpc_loader_high_marker
