/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/PageDirectory.h>
#include <Kernel/Tasks/Thread.h>

namespace Kernel::Memory {

void activate_page_directory(PageDirectory const& directory, Thread* current_thread)
{
    VERIFY(current_thread);
    VERIFY(directory.process());
    current_thread->regs().page_directory = &directory;
    VERIFY(powerpc_page_directory_backend_activate(&directory.backend()));
}

}
