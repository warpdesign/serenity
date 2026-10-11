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
};

struct PowerPCRtasArguments {
    unsigned int token;
    unsigned int argument_count;
    unsigned int result_count;
    unsigned int values[3];
};
