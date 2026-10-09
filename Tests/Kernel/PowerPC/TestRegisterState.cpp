/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/RegisterState.h>
#include <Kernel/Arch/TrapFrame.h>
#include <Kernel/Arch/powerpc/ExceptionState.h>

static_assert(sizeof(Kernel::RegisterState) == sizeof(PowerPCExceptionState));
static_assert(sizeof(Kernel::TrapFrame) == 8);

extern "C" volatile PowerPCExceptionState powerpc_user_probe_frame;

extern "C" int powerpc_register_state_test()
{
    Kernel::RegisterState registers {};
    for (unsigned index = 0; index < 32; ++index)
        registers.gpr[index] = powerpc_user_probe_frame.gpr[index];
    registers.cr = powerpc_user_probe_frame.cr;
    registers.ctr = powerpc_user_probe_frame.ctr;
    registers.xer = powerpc_user_probe_frame.xer;
    registers.lr = powerpc_user_probe_frame.lr;
    registers.srr0 = powerpc_user_probe_frame.srr0;
    registers.srr1 = powerpc_user_probe_frame.srr1;

    if (registers.previous_mode() != Kernel::ExecutionMode::User
        || registers.userspace_sp() != 0x10030ff0u
        || registers.ip() < 0x10010000u || registers.ip() >= 0x10011000u
        || registers.bp() != registers.userspace_sp())
        return 0;

    PtraceRegisters ptrace {};
    Kernel::copy_kernel_registers_into_ptrace_registers(ptrace, registers);
    if (ptrace.pc != registers.ip() || ptrace.gpr[1] != registers.userspace_sp()
        || ptrace.bp() != registers.userspace_sp()
        || ptrace.gpr[3] != registers.gpr[3] || ptrace.msr != registers.srr1)
        return 0;

    ptrace.pc += 4;
    ptrace.gpr[3] = 0x12345678u;
    ptrace.msr &= ~0x4000u;
    Kernel::copy_ptrace_registers_into_kernel_registers(registers, ptrace);
    if (registers.ip() != ptrace.pc || registers.gpr[3] != 0x12345678u
        || registers.previous_mode() != Kernel::ExecutionMode::User)
        return 0;

    registers.gpr[0] = 0x70;
    registers.gpr[3] = 0x11;
    registers.gpr[4] = 0x22;
    registers.gpr[5] = 0x33;
    registers.gpr[6] = 0x44;
    FlatPtr function = 0;
    FlatPtr first = 0;
    FlatPtr second = 0;
    FlatPtr third = 0;
    FlatPtr fourth = 0;
    registers.capture_syscall_params(function, first, second, third, fourth);
    registers.set_return_reg(0xaabbccddu);
    return function == 0x70 && first == 0x11 && second == 0x22
        && third == 0x33 && fourth == 0x44 && registers.gpr[3] == 0xaabbccddu;
}
