/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

void _start(void)
{
    volatile float angle = 0.25f;
    float sine = 0;
    float cosine = 0;
    sincosf(angle, &sine, &cosine);
    if (sine < 0.247f || sine > 0.248f || cosine < 0.968f || cosine > 0.970f)
        exit(1);
    puts("sincosf=ok");
    exit(0);
}
