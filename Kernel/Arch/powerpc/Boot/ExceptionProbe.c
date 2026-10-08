/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

typedef unsigned int Word;

enum {
    ProgramExceptionVector = 0x700,
    VectorSlotSize = 0x100,
    CacheLineSize = 32,
    MSRExternalInterruptEnable = 0x8000,
    MSRProblemState = 0x4000,
    MSRExceptionPrefix = 0x40,
    MSRAddressTranslation = 0x30,
    SRR1TrapException = 0x00020000,
};

extern Word program_exception_vector_start[];
extern Word program_exception_vector_end[];
extern void trigger_program_exception(void);

int powerpc_program_exception_roundtrip(void)
{
    Word machine_state;
    __asm__ volatile("mfmsr %0" : "=r"(machine_state));
    if ((machine_state & (MSRProblemState | MSRAddressTranslation)) != 0)
        return 0; // Require supervisor real mode for direct low-memory vectors.

    Word length = (Word)(__UINTPTR_TYPE__)program_exception_vector_end - (Word)(__UINTPTR_TYPE__)program_exception_vector_start;
    if (length == 0 || length > VectorSlotSize || length % sizeof(Word) != 0)
        return 0;

    Word word_count = length / sizeof(Word);
    for (Word index = 0; index < word_count; ++index) {
        Word address = ProgramExceptionVector + index * sizeof(Word);
        Word instruction = program_exception_vector_start[index];
        __asm__ volatile("stw %0,0(%1)" : : "r"(instruction), "r"(address) : "memory");
    }

    for (Word address = ProgramExceptionVector; address < ProgramExceptionVector + length; address += CacheLineSize)
        __asm__ volatile("dcbst 0,%0" : : "r"(address) : "memory");
    __asm__ volatile("sync" : : : "memory");
    for (Word address = ProgramExceptionVector; address < ProgramExceptionVector + length; address += CacheLineSize)
        __asm__ volatile("icbi 0,%0" : : "r"(address) : "memory");
    __asm__ volatile("sync; isync" : : : "memory");

    Word zero = 0;
    __asm__ volatile("mtspr 273,%0; mtspr 274,%0" : : "r"(zero) : "memory");

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
