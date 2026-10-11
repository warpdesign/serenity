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
#include <Kernel/Security/ExecutionMode.h>
#include <sys/arch/regs.h>

VALIDATE_IS_POWERPC32()

namespace Kernel {

// The base class is the exact integer frame written by the PowerPC exception
// entry. Floating-point registers are saved separately with thread FPU state.
struct RegisterState : PowerPCExceptionState {
    static constexpr bool supports_user_backtrace = true;
    FlatPtr userspace_sp() const { return gpr[1]; }
    void set_userspace_sp(FlatPtr value) { gpr[1] = value; }

    FlatPtr ip() const { return srr0; }
    void set_ip(FlatPtr value) { srr0 = value; }

    // PowerPC stack unwinding follows the r1 backchain rather than r31.
    FlatPtr bp() const { return gpr[1]; }
    void set_bp(FlatPtr value) { gpr[1] = value; }

    ExecutionMode previous_mode() const
    {
        return (srr1 & 0x4000u) ? ExecutionMode::User : ExecutionMode::Kernel;
    }

    void set_return_reg(FlatPtr value) { gpr[3] = value; }

    void capture_syscall_params(FlatPtr& function, FlatPtr& arg1, FlatPtr& arg2, FlatPtr& arg3, FlatPtr& arg4) const
    {
        function = gpr[0];
        arg1 = gpr[3];
        arg2 = gpr[4];
        arg3 = gpr[5];
        arg4 = gpr[6];
    }
};

#define REGISTER_STATE_SIZE (38 * 4)
static_assert(AssertSize<RegisterState, REGISTER_STATE_SIZE>());
static_assert(__builtin_offsetof(RegisterState, srr0) == 144);
static_assert(__builtin_offsetof(RegisterState, srr1) == 148);

inline void copy_kernel_registers_into_ptrace_registers(PtraceRegisters& ptrace, RegisterState const& registers)
{
    for (unsigned index = 0; index < 32; ++index)
        ptrace.gpr[index] = registers.gpr[index];
    ptrace.pc = registers.srr0;
    ptrace.msr = registers.srr1;
    ptrace.lr = registers.lr;
    ptrace.ctr = registers.ctr;
    ptrace.cr = registers.cr;
    ptrace.xer = registers.xer;
}

inline void copy_ptrace_registers_into_kernel_registers(RegisterState& registers, PtraceRegisters const& ptrace)
{
    for (unsigned index = 0; index < 32; ++index)
        registers.gpr[index] = ptrace.gpr[index];
    registers.srr0 = ptrace.pc;
    registers.lr = ptrace.lr;
    registers.ctr = ptrace.ctr;
    registers.cr = ptrace.cr;
    registers.xer = ptrace.xer;
    // Preserve SRR1 privilege and translation bits. ptrace cannot promote a
    // user frame to supervisor mode by changing its saved machine state.
}

struct DebugRegisterState {
};

}
