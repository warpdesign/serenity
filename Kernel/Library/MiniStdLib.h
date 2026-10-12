/*
 * Copyright (c) 2018-2026, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2023-2026, Liav A. <liavalb@hotmail.co.il>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <Kernel/UnixTypes.h>
#include <stddef.h>

extern "C" {

void* memcpy(void*, void const*, size_t);
[[nodiscard]] int strncmp(char const* s1, char const* s2, size_t n);
[[nodiscard]] char* strstr(char const* haystack, char const* needle);
[[nodiscard]] int strcmp(char const*, char const*);
[[nodiscard]] size_t strlen(char const*);
[[nodiscard]] size_t strnlen(char const*, size_t);
void* memset(void*, int, size_t);
[[nodiscard]] int memcmp(void const*, void const*, size_t);
void* memmove(void* dest, void const* src, size_t n);
void const* memmem(void const* haystack, size_t, void const* needle, size_t);

[[nodiscard]] inline u16 ntohs(u16 w)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return __builtin_bswap16(w);
#else
    return w;
#endif
}

[[nodiscard]] inline u16 htons(u16 w) { return ntohs(w); }
}
