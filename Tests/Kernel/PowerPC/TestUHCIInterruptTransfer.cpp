/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Bus/USB/UHCI/UHCIInterruptTransfer.h>

using namespace Kernel::USB;

static constexpr u32 interrupt_in_token = 0x00e00069;
static constexpr u32 completed_status = (1u << 29) | 7u;
static_assert(uhci_token_with_data_toggle(interrupt_in_token, true) == (interrupt_in_token | uhci_td_data_toggle_mask));
static_assert(uhci_token_with_data_toggle(interrupt_in_token | uhci_td_data_toggle_mask, false) == interrupt_in_token);
static_assert(uhci_rearmed_interrupt_status(completed_status)
    == ((1u << 29) | uhci_td_retry_count | uhci_td_active_mask));

int main(int argc, char**)
{
    u32 token = interrupt_in_token;
    token = uhci_token_with_data_toggle(token, true);
    if (!(token & uhci_td_data_toggle_mask))
        return 1;
    token = uhci_token_with_data_toggle(token, false);
    if (token != interrupt_in_token)
        return 1;
    if (argc > 1) {
        auto status = uhci_rearmed_interrupt_status(completed_status | (1u << 19) | (1u << 18));
        if (status & ((1u << 19) | (1u << 18) | 0x7ffu))
            return 1;
    }
    return 0;
}
