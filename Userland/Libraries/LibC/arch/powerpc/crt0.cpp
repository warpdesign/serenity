/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/internals.h>
#include <stdlib.h>
#include <unistd.h>

int main(int, char**, char**);

extern "C" int _entry(int argc, char** argv) __attribute__((used));

asm(R"(
    .section .text
    .p2align 2
    .globl _start
    .type _start,@function
_start:
    # The kernel supplies argc in r3 and argv in r4. Reserve the caller's
    # linkage area before entering C++ on the initial userspace stack.
    addi 1,1,-64
    li 0,0
    stw 0,0(1)
    mtlr 0
    b _entry
    .size _start,.-_start
)");

extern "C" int _entry(int argc, char** argv)
{
    __begin_atexit_locking();
    int status = main(argc, argv, environ);
    exit(status);
    return 20150614;
}
