/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Atomic.h>
#include <LibTest/TestCase.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static AK::Atomic<unsigned> s_destructor_count;
static unsigned s_destructor_order[2];

struct ThreadLocalValue {
    unsigned id;
    ~ThreadLocalValue()
    {
        auto index = s_destructor_count.fetch_add(1);
        if (index < 2)
            s_destructor_order[index] = id;
    }
};

static void* touch_thread_local_values(void*)
{
    thread_local ThreadLocalValue first { 1 };
    thread_local ThreadLocalValue second { 2 };
    (void)first.id;
    (void)second.id;
    return nullptr;
}

TEST_CASE(thread_local_destructors_run_on_pthread_exit_in_reverse_order)
{
    s_destructor_count.store(0);
    s_destructor_order[0] = 0;
    s_destructor_order[1] = 0;
    pthread_t thread;
    auto result = pthread_create(&thread, nullptr, touch_thread_local_values, nullptr);
    EXPECT_EQ(result, 0);
    if (result != 0)
        return;
    EXPECT_EQ(pthread_join(thread, nullptr), 0);
    EXPECT_EQ(s_destructor_count.load(), 2u);
    EXPECT_EQ(s_destructor_order[0], 2u);
    EXPECT_EQ(s_destructor_order[1], 1u);
}

static int s_exit_pipe_write_fd;

struct ProcessExitValue {
    ~ProcessExitValue()
    {
        char marker = '!';
        write(s_exit_pipe_write_fd, &marker, 1);
    }
};

TEST_CASE(thread_local_destructors_run_on_process_exit)
{
    int pipe_fds[2];
    auto pipe_result = pipe(pipe_fds);
    EXPECT_EQ(pipe_result, 0);
    if (pipe_result != 0)
        return;
    auto child = fork();
    EXPECT(child >= 0);
    if (child == 0) {
        close(pipe_fds[0]);
        s_exit_pipe_write_fd = pipe_fds[1];
        thread_local ProcessExitValue value;
        (void)&value;
        exit(0);
    }
    close(pipe_fds[1]);
    if (child < 0) {
        close(pipe_fds[0]);
        return;
    }
    char marker = 0;
    EXPECT_EQ(read(pipe_fds[0], &marker, 1), 1);
    close(pipe_fds[0]);
    int status = 0;
    EXPECT_EQ(waitpid(child, &status, 0), child);
    EXPECT_EQ(status, 0);
    EXPECT_EQ(marker, '!');
}
