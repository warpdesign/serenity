/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Copy a template into a low PowerPC exception slot. In translated mode the
// caller must have a supervisor mapping of the low slot and template.
int powerpc_install_exception_vector(unsigned vector_address, void const* template_start, void const* template_end);

#ifdef __cplusplus
}
#endif
