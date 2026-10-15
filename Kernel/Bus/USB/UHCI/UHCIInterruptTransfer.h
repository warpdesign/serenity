/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>

namespace Kernel::USB {

static constexpr u32 uhci_td_data_toggle_mask = 1u << 19;
static constexpr u32 uhci_td_active_mask = 1u << 23;
static constexpr u32 uhci_td_retry_count = 3u << 27;
static constexpr u32 uhci_td_persistent_status_mask = (1u << 24) | (1u << 25) | (1u << 26) | (1u << 29);

constexpr u32 uhci_token_with_data_toggle(u32 token, bool toggle)
{
    return (token & ~uhci_td_data_toggle_mask) | (toggle ? uhci_td_data_toggle_mask : 0);
}

constexpr u32 uhci_rearmed_interrupt_status(u32 previous_status)
{
    return (previous_status & uhci_td_persistent_status_mask) | uhci_td_retry_count | uhci_td_active_mask;
}

}
