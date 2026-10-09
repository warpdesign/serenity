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

int memcmp(void const* first, void const* second, Size count)
{
    unsigned char const* a = first;
    unsigned char const* b = second;
    for (Size index = 0; index < count; ++index) {
        if (a[index] != b[index])
            return (int)a[index] - (int)b[index];
    }
    return 0;
}

Size strnlen(char const* string, Size maximum)
{
    Size length = 0;
    while (length < maximum && string[length] != '\0')
        ++length;
    return length;
}

__attribute__((noreturn)) void ak_verification_failed(char const* message)
{
    (void)message;
    for (;;) { }
}
