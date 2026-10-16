/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>

namespace Kernel {

// ATA FLUSH CACHE is needed when buffered writes must become durable. Keep
// it pending across ordinary writes and clear it only after a successful flush.
class IDEWriteCacheState {
public:
    void write_completed()
    {
        m_needs_flush = true;
        ++m_pending_writes;
    }
    bool needs_flush() const { return m_needs_flush; }
    u32 pending_writes() const { return m_pending_writes; }
    void flush_completed()
    {
        m_needs_flush = false;
        m_pending_writes = 0;
    }

private:
    bool m_needs_flush { false };
    u32 m_pending_writes { 0 };
};

}
