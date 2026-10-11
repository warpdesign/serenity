/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

struct PowerPCRtasInfo {
    unsigned int runtime_base;
    unsigned int runtime_size;
    unsigned int entry_physical;
    unsigned int arguments_physical;
    unsigned int power_off_token;
    unsigned int system_reboot_token;
    unsigned int get_time_of_day_token;
};

struct PowerPCRtasArguments {
    unsigned int token;
    unsigned int argument_count;
    unsigned int result_count;
    unsigned int values[13];
};

#ifdef __cplusplus
extern "C" {
#endif

unsigned int powerpc_rtas_enter(unsigned int arguments_physical, unsigned int entry_physical,
    unsigned int return_physical, unsigned int return_virtual);
extern unsigned char powerpc_rtas_return_real[];
extern unsigned char powerpc_rtas_return_virtual[];

#ifdef __cplusplus
}
#endif
