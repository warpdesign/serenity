/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibELF/DynamicObject.h>

static_assert(sizeof(Elf_Sym) == 16);
static_assert(sizeof(Elf_Rela) == 12);
static_assert(ELF_ST_TYPE(ELF_ST_INFO(STB_GLOBAL, STT_FUNC)) == STT_FUNC);
static_assert(ELF_ST_BIND(ELF_ST_INFO(STB_GLOBAL, STT_FUNC)) == STB_GLOBAL);
static_assert(ELF_R_SYM(ELF_R_INFO(7ull, 5)) == 7);
static_assert(ELF_R_TYPE(ELF_R_INFO(7ull, 5)) == 5);
