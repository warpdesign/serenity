/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/ExceptionVectorInstaller.h>
#include <Kernel/Arch/powerpc/ExceptionVectorAddresses.h>

typedef unsigned int Word;

enum {
    DataStorageExceptionVector = 0x300,
    ExternalInterruptVector = 0x500,
    ProgramExceptionVector = 0x700,
    DecrementerExceptionVector = 0x900,
    SystemCallExceptionVector = 0xc00,
    MSRExternalInterruptEnable = 0x8000,
    MSRProblemState = 0x4000,
    MSRExceptionPrefix = 0x40,
    MSRAddressTranslation = 0x30,
    SRR1TrapException = 0x00020000,
};

extern Word program_exception_vector_start[];
extern Word program_exception_vector_end[];
extern Word data_storage_exception_vector_start[];
extern Word data_storage_exception_vector_end[];
extern Word external_interrupt_vector_start[];
extern Word external_interrupt_vector_end[];
extern Word decrementer_exception_vector_start[];
extern Word decrementer_exception_vector_end[];
extern Word system_call_probe_vector_start[];
extern Word system_call_probe_vector_end[];
extern Word powerpc_restore_user_exception_state_from_high_real_mode[];
extern Word powerpc_restore_user_exception_state_from_high_real_mode_end[];
extern void trigger_program_exception(void);

int powerpc_install_decrementer_probe_vector(void)
{
    return powerpc_install_exception_vector(DecrementerExceptionVector, decrementer_exception_vector_start, decrementer_exception_vector_end);
}

int powerpc_install_external_interrupt_probe_vector(void)
{
    return powerpc_install_exception_vector(ExternalInterruptVector, external_interrupt_vector_start, external_interrupt_vector_end);
}

int powerpc_program_exception_roundtrip(void)
{
    Word machine_state;
    __asm__ volatile("mfmsr %0" : "=r"(machine_state));
    if ((machine_state & (MSRProblemState | MSRAddressTranslation)) != 0)
        return 0; // Require supervisor real mode for direct low-memory vectors.

    if (!powerpc_install_exception_vector(DataStorageExceptionVector, data_storage_exception_vector_start, data_storage_exception_vector_end)
        || !powerpc_install_exception_vector(ProgramExceptionVector, program_exception_vector_start, program_exception_vector_end)
        || !powerpc_install_exception_vector(SystemCallExceptionVector, system_call_probe_vector_start, system_call_probe_vector_end)
        || !powerpc_install_user_return_trampoline(powerpc_restore_user_exception_state_from_high_real_mode,
            powerpc_restore_user_exception_state_from_high_real_mode_end))
        return 0;

    // Reject invalid slots and templates without changing the installed
    // program vector. The valid vector is exercised by the trap below.
    Word installed_instruction;
    __asm__ volatile("lwz %0,0(%1)" : "=r"(installed_instruction) : "r"(ProgramExceptionVector) : "memory");
    Word oversized_template[65] = { 0 };
    if (powerpc_install_exception_vector(ProgramExceptionVector + 4, program_exception_vector_start, program_exception_vector_end)
        || powerpc_install_exception_vector(ProgramExceptionVector, program_exception_vector_start, program_exception_vector_start)
        || powerpc_install_exception_vector(ProgramExceptionVector, (char*)program_exception_vector_start + 1, program_exception_vector_end)
        || powerpc_install_exception_vector(ProgramExceptionVector, oversized_template, oversized_template + 65))
        return 0;
    Word instruction_after_rejection;
    __asm__ volatile("lwz %0,0(%1)" : "=r"(instruction_after_rejection) : "r"(ProgramExceptionVector) : "memory");
    if (instruction_after_rejection != installed_instruction)
        return 0;

    Word zero = 0;
    __asm__ volatile("mtspr 272,%0; mtspr 273,%0; mtspr 274,%0" : : "r"(zero) : "memory");

    // VOF enters with high vectors selected. Switch to the low vector after
    // firmware access is complete and leave external interrupts disabled.
    machine_state &= ~(MSRExternalInterruptEnable | MSRExceptionPrefix);
    __asm__ volatile("mtmsr %0; isync" : : "r"(machine_state) : "memory");

    trigger_program_exception();

    Word exception_count;
    Word exception_status;
    __asm__ volatile("mfspr %0,273; mfspr %1,274" : "=r"(exception_count), "=r"(exception_status));
    return exception_count == 1 && (exception_status & SRR1TrapException) != 0;
}
