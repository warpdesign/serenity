/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Platform.h>
#include <AK/StdLibExtraDetails.h>
#include <AK/Types.h>
#include <Kernel/Arch/powerpc/ExceptionState.h>

VALIDATE_IS_POWERPC32()

namespace Kernel {

namespace Memory {
class PageDirectory;
}

// Keep the saved GPR and exception status layout identical to RegisterState.
// PowerPC uses segment VSIDs in a shared HPT, so context switching will use
// page_directory to select the address space rather than loading a table root.
struct ThreadRegisters : PowerPCExceptionState {
    Memory::PageDirectory const* page_directory { nullptr };
    FlatPtr kernel_sp { 0 };

    FlatPtr ip() const { return srr0; }
    void set_ip(FlatPtr value) { srr0 = value; }

    FlatPtr sp() const { return gpr[1]; }
    void set_sp(FlatPtr value) { gpr[1] = value; }

    // The stack backchain, rooted at r1, is the PowerPC frame chain.
    FlatPtr frame_pointer() const { return gpr[1]; }

    void set_initial_state(bool is_kernel_process, Memory::PageDirectory const& directory, FlatPtr kernel_stack_top)
    {
        page_directory = &directory;
        kernel_sp = kernel_stack_top;
        set_sp(kernel_stack_top);
        gpr[2] = 0;
        set_machine_state(is_kernel_process);
    }

    void set_entry_function(FlatPtr entry_ip, FlatPtr entry_data)
    {
        set_ip(entry_ip);
        gpr[3] = entry_data;
    }

    void set_exec_state(FlatPtr entry_ip, FlatPtr userspace_sp, Memory::PageDirectory const& directory)
    {
        page_directory = &directory;
        set_ip(entry_ip);
        set_sp(userspace_sp);
        gpr[2] = 0; // The dynamic linker installs the new thread pointer.
        set_machine_state(false);
    }

    void set_return_interrupts_enabled(bool enabled)
    {
        if (enabled)
            srr1 |= 0x8000u;
        else
            srr1 &= ~0x8000u;
    }

private:
    void set_machine_state(bool is_kernel_process)
    {
        // Enable external interrupts, scalar FP, and instruction/data
        // translation. The exception path will restore this through SRR1.
        srr1 = 0xa030u | (is_kernel_process ? 0u : 0x4000u);
    }
};

static_assert(AssertSize<ThreadRegisters, 160>());

}
