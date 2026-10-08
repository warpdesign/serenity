/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

// The Pegasos 2 CPU has a 32-bit effective address space. Keep the upper
// quarter for the kernel and leave the lower three quarters for user space.
// This must be selected by the PowerPC build before including either
// Kernel/Sections.h or Kernel/Prekernel/Prekernel.h.
#define KERNEL_MAPPING_BASE 0xC0000000u

// Pegasos 2 boots through Open Firmware. Its 32-bit boot information must
// not instantiate the 64-bit EFI boot structures.
#define KERNEL_HAS_EFI_BOOT 0
