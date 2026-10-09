/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

.p2align 2
.globl _invoke_entry
.hidden _invoke_entry
.type _invoke_entry,@function

/* r3 = argc, r4 = argv, r5 = envp, r6 = entry. */
_invoke_entry:
    mtctr 6
    bctr

.size _invoke_entry, .-_invoke_entry
