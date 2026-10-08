/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

.text

// BATU = BEPI | (BL << 2) | Vs. A 16 MiB block has BL = 0x7f.
// BATL = BRPN | WIMG | PP. RAM is coherent and read/write (0x12).
// The Pegasos 2 PCI I/O window is cache-inhibited, guarded, and read/write.
.global powerpc_install_boot_bats
.type powerpc_install_boot_bats, @function
powerpc_install_boot_bats:
    li 3, 0
    sync
    mtspr 528, 3    // IBAT0U
    mtspr 530, 3    // IBAT1U
    mtspr 536, 3    // DBAT0U
    mtspr 538, 3    // DBAT1U
    mtspr 540, 3    // DBAT2U
    isync

    li 3, 0x12
    mtspr 529, 3    // IBAT0L: physical RAM 0
    mtspr 531, 3    // IBAT1L: physical RAM 0
    mtspr 537, 3    // DBAT0L: physical RAM 0
    mtspr 539, 3    // DBAT1L: physical RAM 0

    lis 3, 0xfe00
    ori 3, 3, 0x2a
    mtspr 541, 3    // DBAT2L: physical PCI I/O at 0xfe000000

    li 3, 0x1fe
    mtspr 528, 3    // IBAT0U: effective 0x00000000
    mtspr 536, 3    // DBAT0U: effective 0x00000000

    lis 3, 0xc000
    ori 3, 3, 0x1fe
    mtspr 530, 3    // IBAT1U: effective 0xc0000000
    mtspr 538, 3    // DBAT1U: effective 0xc0000000

    lis 3, 0xfe00
    ori 3, 3, 0x1fe
    mtspr 540, 3    // DBAT2U: effective 0xfe000000
    sync
    isync
    blr
.size powerpc_install_boot_bats, .-powerpc_install_boot_bats

.global powerpc_enable_boot_translation
.type powerpc_enable_boot_translation, @function
powerpc_enable_boot_translation:
    mfmsr 3
    ori 3, 3, 0x30 // IR | DR
    sync
    mtmsr 3
    isync
    blr
.size powerpc_enable_boot_translation, .-powerpc_enable_boot_translation

.global powerpc_disable_boot_translation
.type powerpc_disable_boot_translation, @function
powerpc_disable_boot_translation:
    mfmsr 3
    li 4, -49
    and 3, 3, 4    // Clear IR | DR without changing the other MSR bits.
    sync
    mtmsr 3
    isync
    blr
.size powerpc_disable_boot_translation, .-powerpc_disable_boot_translation

.global powerpc_high_alias_marker
.type powerpc_high_alias_marker, @function
powerpc_high_alias_marker:
    tw 31, 0, 0 // Return from the low physical vector into high-mapped code.
    mflr 0
    bl .Lhere
.Lhere:
    mflr 3
    mtlr 0
    lis 4, 0xc000
    cmplw 3, 4
    blt .Lnot_high
    lis 4, 0xc100
    cmplw 3, 4
    bge .Lnot_high
    li 3, 1
    blr
.Lnot_high:
    li 3, 0
    blr
.size powerpc_high_alias_marker, .-powerpc_high_alias_marker
