/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Types.h>
#include <Kernel/Arch/powerpc/ExceptionState.h>
#include <Kernel/Arch/powerpc/HashPageTable.h>
#include <Kernel/Boot/BootInfo.h>

static_assert(ARCH(POWERPC32));
static_assert(KERNEL_HAS_EFI_BOOT == 0);
static_assert(KERNEL_MAPPING_BASE == 0xc0000000u);
static_assert(sizeof(FlatPtr) == 4);
static_assert(sizeof(decltype(Kernel::BootInfo::kernel_mapping_base)) == 4);
static_assert(sizeof(decltype(Kernel::ArchSpecificBootInfo::open_firmware_entry)) == 4);
static_assert(sizeof(PowerPCHPTEntry) == 8);
static_assert(sizeof(PowerPCHashPageTable) == 8);
static_assert(sizeof(PowerPCExceptionState) == 152);
static_assert(__builtin_offsetof(PowerPCExceptionState, cr) == 128);
static_assert(__builtin_offsetof(PowerPCExceptionState, ctr) == 132);
static_assert(__builtin_offsetof(PowerPCExceptionState, lr) == 140);
static_assert(__builtin_offsetof(PowerPCExceptionState, srr0) == 144);
static_assert(__builtin_offsetof(PowerPCExceptionState, srr1) == 148);
