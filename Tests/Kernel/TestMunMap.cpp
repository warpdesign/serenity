/*
 * Copyright (c) 2021-2026, Brian Gianforcaro <bgianf@serenityos.org>
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibTest/TestCase.h>
#include <errno.h>
#include <sys/mman.h>

TEST_CASE(munmap_zero_page)
{
    // munmap of the unmapped zero page should always "succeed".
    auto res = munmap(0x0, 0xF);
    EXPECT_EQ(res, 0);
}

TEST_CASE(protection_change_preserves_page_contents)
{
    auto* memory = static_cast<unsigned char*>(mmap(nullptr, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    EXPECT_NE(memory, MAP_FAILED);
    if (memory == MAP_FAILED)
        return;

    memory[0] = 0x5a;
    int protect_result = mprotect(memory, PAGE_SIZE, PROT_NONE);
    EXPECT_EQ(protect_result, 0);
    if (protect_result != 0) {
        munmap(memory, PAGE_SIZE);
        return;
    }
    protect_result = mprotect(memory, PAGE_SIZE, PROT_READ | PROT_WRITE);
    EXPECT_EQ(protect_result, 0);
    if (protect_result != 0) {
        munmap(memory, PAGE_SIZE);
        return;
    }
    EXPECT_EQ(memory[0], 0x5a);
    EXPECT_EQ(munmap(memory, PAGE_SIZE), 0);
}
