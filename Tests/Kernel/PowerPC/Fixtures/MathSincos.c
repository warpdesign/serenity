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

    volatile double double_angle = 0.25;
    double double_sine = 0;
    double double_cosine = 0;
    sincos(double_angle, &double_sine, &double_cosine);
    if (double_sine < 0.2474 || double_sine > 0.2475 || double_cosine < 0.9689 || double_cosine > 0.9690)
        exit(1);
    puts("sincos=ok");
    exit(0);
}
