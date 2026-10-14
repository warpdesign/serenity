/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/Processor.h>
#include <Kernel/Arch/TrapFrame.h>
#include <Kernel/Arch/powerpc/KernelContext.h>
#include <Kernel/Arch/powerpc/CPU.h>
#include <Kernel/Arch/powerpc/RegisterState.h>
#include <Kernel/Arch/powerpc/Time/Timer.h>
#include <Kernel/Boot/CommandLine.h>
#include <Kernel/Security/Random.h>
#include <Kernel/Tasks/Scheduler.h>

namespace Kernel {

static EntropySource s_decrementer_entropy;
static bool s_insecure_entropy_initialized;
static bool s_insecure_entropy_enabled;
static bool s_insecure_entropy_ready_logged;

extern "C" [[noreturn]] void powerpc_decrementer_exception_handler(RegisterState*);

extern "C" [[noreturn]] void powerpc_decrementer_exception_handler(RegisterState* registers)
{
    VERIFY(registers);
    registers->srr1 &= ~PowerPC::MSR_POW;
    auto previous_mode = registers->previous_mode();
    TrapFrame trap { registers };
    Processor::current().enter_trap(trap, true);
    if (Scheduler::is_initialized()) {
        if (!s_insecure_entropy_initialized) {
            s_insecure_entropy_initialized = true;
            s_insecure_entropy_enabled = kernel_command_line().contains("weakrng"sv);
            if (s_insecure_entropy_enabled)
                dmesgln("PowerPC: Insecure interrupt-timing entropy enabled for bring-up");
        }
        if (s_insecure_entropy_enabled) {
            s_decrementer_entropy.add_random_event(registers->srr0);
            if (!s_insecure_entropy_ready_logged) {
                bool ready;
                {
                    auto& rng = KernelRng::the();
                    SpinlockLocker lock(rng.get_lock());
                    ready = rng.is_ready();
                }
                if (ready) {
                    s_insecure_entropy_ready_logged = true;
                    dmesgln("PowerPC: Insecure interrupt-timing entropy pool ready");
                }
            }
        }
    }
    PowerPC::Timer::the().handle_interrupt();
    Processor::current().exit_trap(trap);
    if (previous_mode == ExecutionMode::User)
        powerpc_return_to_user_from_high(*registers);
    powerpc_restore_supervisor_exception_state(*registers);
}

}
