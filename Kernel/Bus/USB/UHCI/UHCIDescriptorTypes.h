/*
 * Copyright (c) 2021-2026, Jesse Buhagiar <jooster669@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Endian.h>
#include <AK/OwnPtr.h>
#include <AK/Ptr32.h>
#include <AK/Types.h>
#include <Kernel/Bus/USB/UHCI/UHCIInterruptTransfer.h>
#include <Kernel/Bus/USB/USBTransfer.h>

namespace Kernel::USB {

enum class PacketID : u8 {
    IN = 0x69,
    OUT = 0xe1,
    SETUP = 0x2d
};

// Transfer Descriptor register bit offsets/masks
constexpr u16 TD_CONTROL_STATUS_ACTLEN = 0x7ff;
constexpr u8 TD_CONTROL_STATUS_ACTIVE_SHIFT = 23;
constexpr u8 TD_CONTROL_STATUS_INT_ON_COMPLETE_SHIFT = 24;
constexpr u8 TD_CONTROL_STATUS_ISOCHRONOUS_SHIFT = 25;
constexpr u8 TD_CONTROL_STATUS_LS_DEVICE_SHIFT = 26;
constexpr u8 TD_CONTROL_STATUS_ERR_CTR_SHIFT_SHIFT = 27;
constexpr u8 TD_CONTROL_STATUS_SPD_SHIFT = 29;

constexpr u8 TD_TOKEN_PACKET_ID_SHIFT = 0;
constexpr u8 TD_TOKEN_DEVICE_ADDR_SHIFT = 8;
constexpr u8 TD_TOKEN_ENDPOINT_SHIFT = 15;
constexpr u8 TD_TOKEN_DATA_TOGGLE_SHIFT = 19;
constexpr u8 TD_TOKEN_MAXLEN_SHIFT = 21;

//
// Transfer Descriptor
//
// Describes a single transfer event from, or to the Universal Serial Bus.
// These are, generally, attached to Queue Heads, and then executed by the
// USB Host Controller.
// Must be 16-byte aligned
//
struct QueueHead;
struct alignas(16) TransferDescriptor final {
    enum LinkPointerBits {
        Terminate = 1,
        QHSelect = 2,
        DepthFlag = 4,
    };

    enum StatusBits {
        Reserved = (1 << 16),
        BitStuffError = (1 << 17),
        CRCTimeoutError = (1 << 18),
        NAKReceived = (1 << 19),
        BabbleDetected = (1 << 20),
        DataBufferError = (1 << 21),
        Stalled = (1 << 22),
        Active = (1 << 23),
        ErrorMask = BitStuffError | CRCTimeoutError | NAKReceived | BabbleDetected | DataBufferError | Stalled
    };

    enum ControlBits {
        InterruptOnComplete = (1 << 24),
        IsochronousSelect = (1 << 25),
        LowSpeedDevice = (1 << 26),
        ShortPacketDetect = (1 << 29),
    };

    struct TransferDescriptorBookkeeping {
        u32 paddr;                               // Physical 4-byte address where this TransferDescriptor is located
        TransferDescriptor* next_td { nullptr }; // Pointer to first TD
        TransferDescriptor* prev_td { nullptr }; // Pointer to the previous TD
        bool in_use;                             // Has this TD been allocated (and therefore in use)?
    };

    TransferDescriptor() = delete;
    TransferDescriptor(u32 paddr)
    {
        m_bookkeeping = new TransferDescriptorBookkeeping;
        m_bookkeeping->paddr = paddr;
    }
    ~TransferDescriptor() = delete; // Prevent anything except placement new on this object

    u32 link_ptr() const { return m_link_ptr; }
    u32 paddr() const { return m_bookkeeping->paddr; }
    u32 status() const { return AK::convert_between_host_and_little_endian(m_control_status); }
    u32 token() const { return m_token; }
    bool data_toggle() const { return (token() & uhci_td_data_toggle_mask) != 0; }
    u32 buffer_ptr() const { return m_buffer_ptr; }
    u16 actual_packet_length() const { return (status() + 1) & 0x7ff; }

    bool in_use() const { return m_bookkeeping->in_use; }
    bool stalled() const { return status() & StatusBits::Stalled; }
    bool last_in_chain() const { return link_ptr() & LinkPointerBits::Terminate; }
    bool active() const { return status() & StatusBits::Active; }

    void set_active()
    {
        u32 ctrl = status();
        ctrl |= StatusBits::Active;
        set_control_status(ctrl);
    }

    void set_isochronous()
    {
        u32 ctrl = status();
        ctrl |= ControlBits::IsochronousSelect;
        set_control_status(ctrl);
    }

    void set_interrupt_on_complete()
    {
        u32 ctrl = status();
        ctrl |= ControlBits::InterruptOnComplete;
        set_control_status(ctrl);
    }

    void set_lowspeed()
    {
        u32 ctrl = status();
        ctrl |= ControlBits::LowSpeedDevice;
        set_control_status(ctrl);
    }

    void set_error_retry_counter(u8 num_retries)
    {
        VERIFY(num_retries <= 3);
        u32 ctrl = status();
        ctrl |= (num_retries << 27);
        set_control_status(ctrl);
    }

    void set_short_packet_detect()
    {
        u32 ctrl = status();
        ctrl |= ControlBits::ShortPacketDetect;
        set_control_status(ctrl);
    }

    void set_control_status(u32 control_status) { m_control_status = AK::convert_between_host_and_little_endian(control_status); }
    void set_in_use(bool in_use) { m_bookkeeping->in_use = in_use; }
    void set_max_len(u16 max_len)
    {
        VERIFY(max_len < 0x500 || max_len == 0x7ff);
        m_token = token() | (max_len << 21);
    }

    void set_device_endpoint(u8 endpoint)
    {
        VERIFY(endpoint <= 0xf);
        m_token = token() | (endpoint << 18);
    }

    void set_device_address(u8 address)
    {
        VERIFY(address <= 0x7f);
        m_token = token() | (address << 8);
    }

    void set_data_toggle(bool toggle)
    {
        m_token = uhci_token_with_data_toggle(token(), toggle);
    }

    void set_packet_id(PacketID pid) { m_token = token() | static_cast<u32>(pid); }
    void link_queue_head(u32 qh_paddr)
    {
        m_link_ptr = qh_paddr;
        m_link_ptr = link_ptr() | LinkPointerBits::QHSelect;
    }

    void print()
    {
        dbgln("UHCI: TD({:#04x}) @ {:#04x}: link_ptr={:#04x}, status={:#04x}, token={:#04x}, buffer_ptr={:#04x}", this, m_bookkeeping->paddr, link_ptr(), status(), token(), buffer_ptr());

        // Now let's print the flags!
        dbgln("UHCI: TD({:#04x}) @ {:#04x}: link_ptr={}{}{}, status={}{}{}{}{}{}{}",
            this,
            m_bookkeeping->paddr,
            (last_in_chain()) ? "T " : "",
            (link_ptr() & static_cast<u32>(LinkPointerBits::QHSelect)) ? "QH " : "",
            (link_ptr() & static_cast<u32>(LinkPointerBits::DepthFlag)) ? "Vf " : "",
            (status() & static_cast<u32>(StatusBits::BitStuffError)) ? "BITSTUFF " : "",
            (status() & static_cast<u32>(StatusBits::CRCTimeoutError)) ? "CRCTIMEOUT " : "",
            (status() & static_cast<u32>(StatusBits::NAKReceived)) ? "NAK " : "",
            (status() & static_cast<u32>(StatusBits::BabbleDetected)) ? "BABBLE " : "",
            (status() & static_cast<u32>(StatusBits::DataBufferError)) ? "DATAERR " : "",
            (stalled()) ? "STALL " : "",
            (active()) ? "ACTIVE " : "");
    }

    // FIXME: For the love of God, use AK SMART POINTERS PLEASE!!
    TransferDescriptor* next_td() { return m_bookkeeping->next_td; }
    TransferDescriptor const* next_td() const { return m_bookkeeping->next_td; }
    void set_next_td(TransferDescriptor* td) { m_bookkeeping->next_td = td; }

    TransferDescriptor* prev_td() { return m_bookkeeping->prev_td; }
    TransferDescriptor const* prev_td() const { return m_bookkeeping->prev_td; }
    void set_previous_td(TransferDescriptor* td) { m_bookkeeping->prev_td = td; }

    void insert_next_transfer_descriptor(TransferDescriptor* td)
    {
        m_link_ptr = td->paddr();
        td->set_previous_td(this);
        set_next_td(td);

        // Let's set some bits for the link ptr
        m_link_ptr = link_ptr() | static_cast<u32>(LinkPointerBits::DepthFlag);
    }

    void terminate() { m_link_ptr = link_ptr() | static_cast<u32>(LinkPointerBits::Terminate); }

    void set_buffer_address(Ptr32<u8> buffer)
    {
        u8* buffer_address = &*buffer;
        m_buffer_ptr = reinterpret_cast<uintptr_t>(buffer_address);
    }

    // DEBUG FUNCTIONS!
    void set_token(u32 token)
    {
        m_token = token;
    }

    void set_status(u32 status)
    {
        set_control_status(status);
    }

    void free()
    {
        m_link_ptr = 0;
        m_control_status = 0;
        m_token = 0;
        m_bookkeeping->in_use = false;
        m_bookkeeping->next_td = nullptr;
        m_bookkeeping->prev_td = nullptr;
    }

private:
    LittleEndian<u32> m_link_ptr;   // Points to another Queue Head or Transfer Descriptor
    u32 volatile m_control_status; // Control and status field, in little-endian byte order
    LittleEndian<u32> m_token;      // Contains all information required to fill in a USB Start Token
    LittleEndian<u32> m_buffer_ptr; // Points to a data buffer for this transaction (i.e what we want to send or recv)

    // This structure pointer will be ignored by the controller, but we can use it for configuration and bookkeeping
    TransferDescriptorBookkeeping* m_bookkeeping { nullptr };
};

static_assert(AssertSize<TransferDescriptor, 32>()); // Transfer Descriptor is always 8 Dwords

//
// Queue Head
//
// Description here please!
//
struct alignas(16) QueueHead {

    struct QueueHeadBookkeeping {
        u32 paddr { 0 };                          // Physical 4-byte address where this QueueHead is located
        QueueHead* next_qh { nullptr };           // Next QH
        QueueHead* prev_qh { nullptr };           // Previous QH
        TransferDescriptor* first_td { nullptr }; // Pointer to first TD
        Transfer* transfer { nullptr };           // Pointer to transfer linked to this queue head
        bool in_use { false };                    // Is this QH currently in use?
    };

    // The number of padding bytes is the size of a full QueueHead descriptor (32-bytes), minus the two required members (8-bytes), minus the size
    // of a pointer to the bookkeeping structure.
    static constexpr size_t DESCRIPTOR_PAD_BYTES = 32u - 8 - sizeof(QueueHeadBookkeeping*);

    enum class LinkPointerBits : u32 {
        Terminate = 1,
        QHSelect = 2,
    };

    QueueHead() = delete;
    QueueHead(u32 paddr)
    {
        m_bookkeeping = new QueueHeadBookkeeping;
        m_bookkeeping->paddr = paddr;
    }
    ~QueueHead() = delete; // Prevent anything except placement new on this object

    u32 link_ptr() const { return m_link_ptr; }
    u32 element_link_ptr() const { return AK::convert_between_host_and_little_endian(m_element_link_ptr); }

    u32 paddr() const { return m_bookkeeping->paddr; }
    bool in_use() const { return m_bookkeeping->in_use; }
    void set_in_use(bool in_use) { m_bookkeeping->in_use = in_use; }

    QueueHead* next_qh() { return m_bookkeeping->next_qh; }
    QueueHead const* next_qh() const { return m_bookkeeping->next_qh; }

    QueueHead* prev_qh() { return m_bookkeeping->prev_qh; }
    QueueHead const* prev_qh() const { return m_bookkeeping->prev_qh; }

    void link_next_queue_head(QueueHead* qh)
    {
        m_link_ptr = qh->paddr();
        m_link_ptr = link_ptr() | static_cast<u32>(LinkPointerBits::QHSelect);
        m_bookkeeping->next_qh = qh;
        qh->m_bookkeeping->prev_qh = this;
    }

    void attach_transfer_queue(QueueHead& qh)
    {
        m_element_link_ptr = AK::convert_between_host_and_little_endian(qh.paddr() | static_cast<u32>(LinkPointerBits::QHSelect));
    }

    // TODO: Should we pass in an array or vector of TDs instead????
    void attach_transfer_descriptor_chain(TransferDescriptor* td)
    {
        m_bookkeeping->first_td = td;
        m_element_link_ptr = AK::convert_between_host_and_little_endian(td->paddr());
    }

    TransferDescriptor* get_first_td()
    {
        return m_bookkeeping->first_td;
    }

    void terminate() { m_link_ptr = link_ptr() | static_cast<u32>(LinkPointerBits::Terminate); }

    void terminate_element_link_ptr()
    {
        m_element_link_ptr = AK::convert_between_host_and_little_endian(static_cast<u32>(LinkPointerBits::Terminate));
    }

    void set_transfer(Transfer* transfer)
    {
        m_bookkeeping->transfer = transfer;
    }

    Transfer* transfer()
    {
        return m_bookkeeping->transfer;
    }

    void print()
    {
        dbgln("UHCI: QH({:#04x}) @ {:#04x}: link_ptr={:#04x}, element_link_ptr={:#04x}", this, m_bookkeeping->paddr, link_ptr(), element_link_ptr());
    }

    void free()
    {
        m_link_ptr = 0;
        m_element_link_ptr = 0;
        m_bookkeeping->first_td = nullptr;
        m_bookkeeping->transfer = nullptr;
        m_bookkeeping->next_qh = nullptr;
        m_bookkeeping->prev_qh = nullptr;
        m_bookkeeping->in_use = false;
    }

    void reinitialize(bool advance_data_toggle)
    {
        auto* td = get_first_td();
        VERIFY(td);
        // Interrupt pipes currently submit one packet per transfer. Preserve
        // the toggle on a NAK/error; advance it after a successful report.
        VERIFY(!td->next_td());
        if (advance_data_toggle)
            td->set_data_toggle(!td->data_toggle());
        td->set_status(uhci_rearmed_interrupt_status(td->status()));
        attach_transfer_descriptor_chain(td);
    }

private:
    LittleEndian<u32> m_link_ptr { 0 };     // Pointer to the next horizontal object that the controller will execute after this one
    u32 volatile m_element_link_ptr { 0 }; // Pointer to the first data object in the queue (can be modified by hw), in little-endian byte order

    // This structure pointer will be ignored by the controller, but we can use it for configuration and bookkeeping
    QueueHeadBookkeeping* m_bookkeeping { nullptr };
    u8 m_padding[DESCRIPTOR_PAD_BYTES];
};

static_assert(AssertSize<QueueHead, 32>()); // Queue Head is always 8 Dwords

struct AsyncTransferHandle {
    NonnullLockRefPtr<Transfer> transfer;
    QueueHead* qh;
    u16 ms_poll_interval;
    u32 last_error_status { 0 };
};

}
