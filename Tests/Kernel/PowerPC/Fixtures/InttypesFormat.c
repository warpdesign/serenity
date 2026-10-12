/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void _start(void)
{
    char text[32];
    uint64_t value = UINT64_MAX;
    if (snprintf(text, sizeof(text), "%" PRIu64, value) != 20
        || strcmp(text, "18446744073709551615") != 0) {
        puts("64-bit format failed");
        exit(1);
    }

    uint64_t parsed = 0;
    if (sscanf(text, "%" SCNu64, &parsed) != 1 || parsed != UINT64_MAX) {
        puts("64-bit scan failed");
        exit(1);
    }

    puts("u64=18446744073709551615");
    exit(0);
}
