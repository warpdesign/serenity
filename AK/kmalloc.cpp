/*
 * Copyright (c) 2018-2026, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2021-2026, Daniel Bertalan <dani@danielbertalan.dev>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/kmalloc.h>

#if defined(AK_OS_SERENITY) && !defined(KERNEL)

#    include <AK/Assertions.h>

// However deceptively simple these functions look, they must not be inlined.
// Memory allocated in one translation unit has to be deallocatable in another
// translation unit, so these functions must be the same everywhere.
// By making these functions global, this invariant is enforced.

void* operator new(size_t size)
{
    void* ptr = malloc(size);
    VERIFY(ptr);
    return ptr;
}

void* operator new(size_t size, std::nothrow_t const&) noexcept
{
    return malloc(size);
}

void operator delete(void* ptr) noexcept
{
    return free(ptr);
}

void operator delete(void* ptr, size_t) noexcept
{
    return free(ptr);
}

void* operator new[](size_t size)
{
    void* ptr = malloc(size);
    VERIFY(ptr);
    return ptr;
}

void* operator new[](size_t size, std::nothrow_t const&) noexcept
{
    return malloc(size);
}

void operator delete[](void* ptr) noexcept
{
    return free(ptr);
}

void operator delete[](void* ptr, size_t) noexcept
{
    return free(ptr);
}

static void* allocate_aligned(size_t size, std::align_val_t alignment)
{
    void* ptr = nullptr;
    if (posix_memalign(&ptr, static_cast<size_t>(alignment), max(size, size_t { 1 })) != 0)
        return nullptr;
    return ptr;
}

void* operator new(size_t size, std::align_val_t alignment)
{
    auto* ptr = allocate_aligned(size, alignment);
    VERIFY(ptr);
    return ptr;
}

void* operator new(size_t size, std::align_val_t alignment, std::nothrow_t const&) noexcept
{
    return allocate_aligned(size, alignment);
}

void* operator new[](size_t size, std::align_val_t alignment)
{
    return ::operator new(size, alignment);
}

void* operator new[](size_t size, std::align_val_t alignment, std::nothrow_t const&) noexcept
{
    return allocate_aligned(size, alignment);
}

void operator delete(void* ptr, std::align_val_t) noexcept
{
    free(ptr);
}

void operator delete(void* ptr, size_t, std::align_val_t) noexcept
{
    free(ptr);
}

void operator delete(void* ptr, std::align_val_t, std::nothrow_t const&) noexcept
{
    free(ptr);
}

void operator delete[](void* ptr, std::align_val_t) noexcept
{
    free(ptr);
}

void operator delete[](void* ptr, size_t, std::align_val_t) noexcept
{
    free(ptr);
}

void operator delete[](void* ptr, std::align_val_t, std::nothrow_t const&) noexcept
{
    free(ptr);
}

// This is usually provided by libstdc++ in most cases, and the kernel has its own definition in
// Kernel/Heap/kmalloc.cpp. If neither of those apply, the following should suffice to not fail during linking.
namespace AK_REPLACED_STD_NAMESPACE {
nothrow_t const nothrow;
}

#endif
