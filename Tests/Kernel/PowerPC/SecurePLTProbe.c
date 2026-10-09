/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

extern int imported_function(int, double);
extern int second_imported_function(int, double);

int call_imported_function(int value, double fraction)
{
    return imported_function(value + 1, fraction + 1.5) + 7;
}

int call_second_imported_function(int value, double fraction)
{
    return second_imported_function(value + 2, fraction + 2.5) + 9;
}
