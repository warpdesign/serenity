/*
 * Copyright (c) 2018-2026, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Endian.h>
#include <AK/Types.h>

namespace DNS {

class [[gnu::packed]] PacketHeader {
public:
    PacketHeader() = default;

    u16 id() const { return m_id; }
    void set_id(u16 w) { m_id = w; }

    bool recursion_desired() const { return m_flags1 & 0x01; }
    void set_recursion_desired(bool b) { set_flag(m_flags1, 0x01, b); }

    bool is_truncated() const { return m_flags1 & 0x02; }
    void set_truncated(bool b) { set_flag(m_flags1, 0x02, b); }

    bool is_authoritative_answer() const { return m_flags1 & 0x04; }
    void set_authoritative_answer(bool b) { set_flag(m_flags1, 0x04, b); }

    u8 opcode() const { return (m_flags1 >> 3) & 0x0f; }
    void set_opcode(u8 b) { m_flags1 = (m_flags1 & 0x87) | ((b & 0x0f) << 3); }

    bool is_query() const { return !(m_flags1 & 0x80); }
    bool is_response() const { return m_flags1 & 0x80; }
    void set_is_query() { m_flags1 &= ~0x80; }
    void set_is_response() { m_flags1 |= 0x80; }

    u8 response_code() const { return m_flags2 & 0x0f; }
    void set_response_code(u8 b) { m_flags2 = (m_flags2 & 0xf0) | (b & 0x0f); }

    bool checking_disabled() const { return m_flags2 & 0x10; }
    void set_checking_disabled(bool b) { set_flag(m_flags2, 0x10, b); }

    bool is_authenticated_data() const { return m_flags2 & 0x20; }
    void set_authenticated_data(bool b) { set_flag(m_flags2, 0x20, b); }

    bool is_recursion_available() const { return m_flags2 & 0x80; }
    void set_recursion_available(bool b) { set_flag(m_flags2, 0x80, b); }

    u16 question_count() const { return m_question_count; }
    void set_question_count(u16 w) { m_question_count = w; }

    u16 answer_count() const { return m_answer_count; }
    void set_answer_count(u16 w) { m_answer_count = w; }

    u16 authority_count() const { return m_authority_count; }
    void set_authority_count(u16 w) { m_authority_count = w; }

    u16 additional_count() const { return m_additional_count; }
    void set_additional_count(u16 w) { m_additional_count = w; }

    void* payload() { return this + 1; }
    void const* payload() const { return this + 1; }

private:
    static void set_flag(u8& flags, u8 mask, bool enabled)
    {
        if (enabled)
            flags |= mask;
        else
            flags &= ~mask;
    }

    NetworkOrdered<u16> m_id;
    // DNS flag bits are defined by wire position, independent of host bitfield layout.
    u8 m_flags1 { 0 }; // QR, opcode, AA, TC, RD
    u8 m_flags2 { 0 }; // RA, Z, AD, CD, response code

    NetworkOrdered<u16> m_question_count;
    NetworkOrdered<u16> m_answer_count;
    NetworkOrdered<u16> m_authority_count;
    NetworkOrdered<u16> m_additional_count;
};

static_assert(sizeof(PacketHeader) == 12);

}

template<>
struct AK::Traits<DNS::PacketHeader> : public AK::DefaultTraits<DNS::PacketHeader> {
    static constexpr bool is_trivially_serializable() { return true; }
};
