/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

typedef unsigned int PowerPCRegisterWord;

// Integer state saved by the 32-bit PowerPC exception entry. Floating-point
// and vector state require separate handling before this is a thread context.
struct PowerPCExceptionState {
    PowerPCRegisterWord gpr[32];
    PowerPCRegisterWord cr;
    PowerPCRegisterWord ctr;
    PowerPCRegisterWord xer;
    PowerPCRegisterWord lr;
    PowerPCRegisterWord srr0;
    PowerPCRegisterWord srr1;
};
