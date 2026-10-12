/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibDNS/Packet.h>
#include <LibDNS/PacketHeader.h>
#include <LibTest/TestCase.h>

TEST_CASE(query_header_uses_dns_wire_bit_positions)
{
    DNS::PacketHeader header;
    header.set_id(0x1234);
    header.set_is_query();
    header.set_recursion_desired(true);
    header.set_question_count(1);

    auto const* bytes = reinterpret_cast<u8 const*>(&header);
    EXPECT_EQ(bytes[0], 0x12);
    EXPECT_EQ(bytes[1], 0x34);
    EXPECT_EQ(bytes[2], 0x01);
    EXPECT_EQ(bytes[3], 0x00);
    EXPECT_EQ(bytes[4], 0x00);
    EXPECT_EQ(bytes[5], 0x01);

    DNS::Packet query;
    query.set_id(0x1234);
    auto buffer = MUST(query.to_byte_buffer());
    EXPECT_EQ(buffer[2], 0x01);
    EXPECT_EQ(buffer[3], 0x00);
}

TEST_CASE(response_header_decodes_dns_wire_bit_positions)
{
    u8 bytes[12] { 0x12, 0x34, 0x85, 0xa3, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00 };
    auto const& header = *reinterpret_cast<DNS::PacketHeader const*>(bytes);
    EXPECT_EQ(header.id(), 0x1234);
    EXPECT(header.is_response());
    EXPECT(header.is_authoritative_answer());
    EXPECT(header.recursion_desired());
    EXPECT(header.is_recursion_available());
    EXPECT(header.is_authenticated_data());
    EXPECT_EQ(header.response_code(), 3);
    EXPECT_EQ(header.question_count(), 1);
    EXPECT_EQ(header.answer_count(), 2);
}
