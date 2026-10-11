/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Atomic.h>
#include <LibTest/TestCase.h>
#include <pthread.h>

static AK::Atomic<unsigned> s_construction_count;

struct LocalStaticValue {
    LocalStaticValue()
    {
        s_construction_count.fetch_add(1);
    }
    unsigned value { 42 };
};

static LocalStaticValue& value()
{
    static LocalStaticValue instance;
    return instance;
}

static void* read_value(void*)
{
    return reinterpret_cast<void*>(static_cast<uintptr_t>(value().value));
}

TEST_CASE(local_static_constructed_once_across_threads)
{
    pthread_t first;
    pthread_t second;
    auto first_result = pthread_create(&first, nullptr, read_value, nullptr);
    EXPECT_EQ(first_result, 0);
    if (first_result != 0)
        return;
    auto second_result = pthread_create(&second, nullptr, read_value, nullptr);
    EXPECT_EQ(second_result, 0);
    if (second_result != 0) {
        pthread_join(first, nullptr);
        return;
    }
    void* first_value = nullptr;
    void* second_value = nullptr;
    EXPECT_EQ(pthread_join(first, &first_value), 0);
    EXPECT_EQ(pthread_join(second, &second_value), 0);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(first_value), 42u);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(second_value), 42u);
    EXPECT_EQ(s_construction_count.load(), 1u);
}
