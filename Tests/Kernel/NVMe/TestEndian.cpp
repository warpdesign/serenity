/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Devices/Storage/NVMe/NVMeDefinitions.h>
#include <Kernel/Devices/Storage/NVMe/NVMeEndian.h>

static_assert(sizeof(NVMeSubmission) == 64);
static_assert(sizeof(NVMeCompletion) == 16);
static_assert(sizeof(IdentifyNamespace) == NVMe_IDENTIFY_SIZE);
static_assert(sizeof(IdentifyController) == NVMe_IDENTIFY_SIZE);

static bool bytes_equal(u8 const* actual, u8 const* expected, size_t count)
{
    for (size_t index = 0; index < count; ++index) {
        if (actual[index] != expected[index])
            return false;
    }
    return true;
}

extern "C" int nvme_endian_test()
{
    NVMeSubmission read {};
    read.op = OP_NVME_READ;
    read.cmdid = 0x1234;
    read.rw.nsid = 0x11223344;
    read.rw.data_ptr.prp1 = 0x0123456789abcdefull;
    read.rw.slba = 0xfedcba9876543210ull;
    read.rw.length = 0x3456;
    auto const* read_bytes = reinterpret_cast<u8 const*>(&read);
    u8 const expected_id[] = { 0x34, 0x12 };
    u8 const expected_namespace[] = { 0x44, 0x33, 0x22, 0x11 };
    u8 const expected_prp[] = { 0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 0x01 };
    u8 const expected_lba[] = { 0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe };
    u8 const expected_length[] = { 0x56, 0x34 };
    if (read_bytes[0] != OP_NVME_READ || !bytes_equal(read_bytes + 2, expected_id, 2)
        || !bytes_equal(read_bytes + 4, expected_namespace, 4)
        || !bytes_equal(read_bytes + 24, expected_prp, 8)
        || !bytes_equal(read_bytes + 40, expected_lba, 8)
        || !bytes_equal(read_bytes + 48, expected_length, 2))
        return 0;

    NVMeSubmission create_queue {};
    create_queue.create_cq.prp1 = 0x0123456789abcdefull;
    create_queue.create_cq.qsize = 63;
    create_queue.create_cq.cq_flags = QUEUE_PHY_CONTIGUOUS | QUEUE_IRQ_ENABLED;
    auto const* queue_bytes = reinterpret_cast<u8 const*>(&create_queue);
    u8 const expected_queue_fields[] = { 0x3f, 0x00, 0x03, 0x00 };
    if (!bytes_equal(queue_bytes + 24, expected_prp, 8)
        || !bytes_equal(queue_bytes + 42, expected_queue_fields, 4))
        return 0;

    NVMeCompletion completion {};
    completion.command_id = 0x1234;
    completion.status = 0x5678;
    auto const* completion_bytes = reinterpret_cast<u8 const*>(&completion);
    u8 const expected_completion[] = { 0x34, 0x12, 0x78, 0x56 };
    if (!bytes_equal(completion_bytes + 12, expected_completion, 4))
        return 0;
    if (completion.command_id != 0x1234 || completion.status != 0x5678)
        return 0;

    IdentifyNamespace identify {};
    auto* identify_bytes = reinterpret_cast<u8*>(&identify);
    for (size_t index = 0; index < 8; ++index)
        identify_bytes[index] = expected_prp[index];
    if (identify.nsze != 0x0123456789abcdefull)
        return 0;
    IdentifyController controller {};
    auto* controller_bytes = reinterpret_cast<u8*>(&controller);
    controller_bytes[256] = 0x34;
    controller_bytes[257] = 0x12;
    if (controller.oacs != 0x1234)
        return 0;

    volatile u64 register64 = 0;
    Kernel::write_nvme_register(register64, 0x0123456789abcdefull);
    auto const* register64_bytes = reinterpret_cast<volatile u8 const*>(&register64);
    for (size_t index = 0; index < 8; ++index) {
        if (register64_bytes[index] != expected_prp[index])
            return 0;
    }
    if (Kernel::read_nvme_register(register64) != 0x0123456789abcdefull)
        return 0;

    volatile u32 register32 = 0;
    Kernel::write_nvme_register(register32, 0x11223344u);
    auto const* register32_bytes = reinterpret_cast<volatile u8 const*>(&register32);
    return bytes_equal(read_bytes + 24, expected_prp, 8)
        && register32_bytes[0] == 0x44 && register32_bytes[1] == 0x33
        && register32_bytes[2] == 0x22 && register32_bytes[3] == 0x11
        && Kernel::read_nvme_register(register32) == 0x11223344u;
}
