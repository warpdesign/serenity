/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

namespace Kernel {

struct ThreadRegisters;
struct ArchSpecificThreadData;
namespace Memory {
class PageDirectory;
}
namespace Syscall {
struct SC_create_thread_params;
}

void prepare_user_thread_registers(ThreadRegisters&, ArchSpecificThreadData&, Memory::PageDirectory const&, Syscall::SC_create_thread_params const&);

}
