/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

typedef __SIZE_TYPE__ Size;

void* memcpy(void* destination, void const* source, Size count)
{
    volatile unsigned char* dest = destination;
    unsigned char const* src = source;
    for (Size index = 0; index < count; ++index)
        dest[index] = src[index];
    return destination;
}

void* memset(void* destination, int value, Size count)
{
    volatile unsigned char* dest = destination;
    for (Size index = 0; index < count; ++index)
        dest[index] = (unsigned char)value;
    return destination;
}

__attribute__((noreturn)) void ak_verification_failed(char const* message)
{
    (void)message;
    for (;;) { }
}
