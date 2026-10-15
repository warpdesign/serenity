/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

// memcpy, memmove and memset for 32-bit PowerPC, shared by the kernel, LibC and the dynamic loader.
// Destinations may be cache-inhibited (framebuffers), so these never use dcbz and only make aligned
// word accesses there. Buffers whose addresses differ in their low two bits are copied bytewise.

.text

.global memcpy
.type memcpy, @function
memcpy:
    mr 6, 3
.Lforward:
    cmplwi 5, 16
    blt .Lforward_bytes
    xor 0, 6, 4
    andi. 0, 0, 3
    bne .Lforward_bytes
    andi. 0, 6, 3
    beq .Lforward_aligned
    subfic 0, 0, 4
    sub 5, 5, 0
    mtctr 0
1:  lbz 7, 0(4)
    addi 4, 4, 1
    stb 7, 0(6)
    addi 6, 6, 1
    bdnz 1b
.Lforward_aligned:
    srwi. 0, 5, 5
    beq .Lforward_words
    mtctr 0
    li 12, 64
2:  dcbt 12, 4
    lwz 7, 0(4)
    lwz 8, 4(4)
    lwz 9, 8(4)
    lwz 10, 12(4)
    stw 7, 0(6)
    stw 8, 4(6)
    stw 9, 8(6)
    stw 10, 12(6)
    lwz 7, 16(4)
    lwz 8, 20(4)
    lwz 9, 24(4)
    lwz 10, 28(4)
    stw 7, 16(6)
    stw 8, 20(6)
    stw 9, 24(6)
    stw 10, 28(6)
    addi 4, 4, 32
    addi 6, 6, 32
    bdnz 2b
.Lforward_words:
    rlwinm. 0, 5, 30, 29, 31
    beq .Lforward_tail
    mtctr 0
3:  lwz 7, 0(4)
    addi 4, 4, 4
    stw 7, 0(6)
    addi 6, 6, 4
    bdnz 3b
.Lforward_tail:
    andi. 5, 5, 3
.Lforward_bytes:
    cmplwi 5, 0
    beqlr
    mtctr 5
4:  lbz 7, 0(4)
    addi 4, 4, 1
    stb 7, 0(6)
    addi 6, 6, 1
    bdnz 4b
    blr
.size memcpy, . - memcpy

.global memmove
.type memmove, @function
memmove:
    mr 6, 3
    // A forward copy is safe unless the destination starts inside the source.
    subf 0, 4, 3
    cmplw 0, 5
    bge .Lforward
    add 4, 4, 5
    add 6, 3, 5
    cmplwi 5, 16
    blt .Lbackward_bytes
    xor 0, 6, 4
    andi. 0, 0, 3
    bne .Lbackward_bytes
    andi. 0, 6, 3
    beq .Lbackward_aligned
    sub 5, 5, 0
    mtctr 0
1:  lbzu 7, -1(4)
    stbu 7, -1(6)
    bdnz 1b
.Lbackward_aligned:
    srwi. 0, 5, 4
    beq .Lbackward_words
    mtctr 0
2:  lwz 7, -4(4)
    lwz 8, -8(4)
    lwz 9, -12(4)
    lwzu 10, -16(4)
    stw 7, -4(6)
    stw 8, -8(6)
    stw 9, -12(6)
    stwu 10, -16(6)
    bdnz 2b
.Lbackward_words:
    rlwinm. 0, 5, 30, 30, 31
    beq .Lbackward_tail
    mtctr 0
3:  lwzu 7, -4(4)
    stwu 7, -4(6)
    bdnz 3b
.Lbackward_tail:
    andi. 5, 5, 3
.Lbackward_bytes:
    cmplwi 5, 0
    beqlr
    mtctr 5
4:  lbzu 7, -1(4)
    stbu 7, -1(6)
    bdnz 4b
    blr
.size memmove, . - memmove

.global memset
.type memset, @function
memset:
    mr 6, 3
    rlwinm 4, 4, 0, 24, 31
    rlwimi 4, 4, 8, 16, 23
    rlwimi 4, 4, 16, 0, 15
    cmplwi 5, 16
    blt .Lset_bytes
    andi. 0, 6, 3
    beq .Lset_aligned
    subfic 0, 0, 4
    sub 5, 5, 0
    mtctr 0
1:  stb 4, 0(6)
    addi 6, 6, 1
    bdnz 1b
.Lset_aligned:
    srwi. 0, 5, 5
    beq .Lset_words
    mtctr 0
2:  stw 4, 0(6)
    stw 4, 4(6)
    stw 4, 8(6)
    stw 4, 12(6)
    stw 4, 16(6)
    stw 4, 20(6)
    stw 4, 24(6)
    stw 4, 28(6)
    addi 6, 6, 32
    bdnz 2b
.Lset_words:
    rlwinm. 0, 5, 30, 29, 31
    beq .Lset_tail
    mtctr 0
3:  stw 4, 0(6)
    addi 6, 6, 4
    bdnz 3b
.Lset_tail:
    andi. 5, 5, 3
.Lset_bytes:
    cmplwi 5, 0
    beqlr
    mtctr 5
4:  stb 4, 0(6)
    addi 6, 6, 1
    bdnz 4b
    blr
.size memset, . - memset
