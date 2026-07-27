// uart_receiver.cpp
#include "uart_receiver.h"
#include "ota_protocol.h"
#include "signature_verify.h"
#include "rollback.h"
#include "uart2.h"
#include "system_init.h"
#include "libc_shim.h"
#include "led.h"

namespace boot {

namespace {

void send_ack_or_nack(bool ack, uint16_t seq) {
    ota::PacketHeader hdr{
        ota::SYNC_BYTE,
        static_cast<uint8_t>(ack ? ota::PacketType::ACK : ota::PacketType::NACK),
        seq, 0};
    uint8_t frame[ota::HEADER_SIZE + ota::CRC_SIZE];
    memcpy(frame, &hdr, ota::HEADER_SIZE);
    uint32_t crc = ota::Crc32::compute(frame, ota::HEADER_SIZE);
    memcpy(frame + ota::HEADER_SIZE, &crc, sizeof(crc));
    uart2::send_buffer(frame, sizeof(frame));
}

// Blocking receive of exactly one framed packet, with a timeout. Returns
// false on timeout or CRC/sync failure (caller should NACK and retry).
bool receive_packet(uint8_t* payload_out, uint16_t* payload_len_out,
                     ota::PacketType* type_out, uint16_t* seq_out,
                     uint32_t timeout_ms) {
    ota::PacketHeader hdr{};
    if (!uart2::receive_buffer(reinterpret_cast<uint8_t*>(&hdr), ota::HEADER_SIZE, timeout_ms)) {
        return false;
    }
    if (hdr.sync != ota::SYNC_BYTE || hdr.length > ota::MAX_PAYLOAD) {
        return false;
    }

    uint8_t buf[ota::MAX_PACKET_SIZE];
    memcpy(buf, &hdr, ota::HEADER_SIZE);

    if (hdr.length > 0) {
        if (!uart2::receive_buffer(buf + ota::HEADER_SIZE, hdr.length, timeout_ms)) {
            return false;
        }
    }

    uint32_t crc_received;
    if (!uart2::receive_buffer(reinterpret_cast<uint8_t*>(&crc_received), sizeof(crc_received), timeout_ms)) {
        return false;
    }

    uint32_t crc_calc = ota::Crc32::compute(buf, ota::HEADER_SIZE + hdr.length);
    if (crc_received != crc_calc) {
        return false;
    }

    *type_out = static_cast<ota::PacketType>(hdr.type);
    *seq_out = hdr.seq;
    *payload_len_out = hdr.length;
    if (hdr.length > 0) {
        memcpy(payload_out, buf + ota::HEADER_SIZE, hdr.length);
    }

    // flickering ld2 led to show that it's working
    led::on();
    sys::delay_ms(150);
    led::off();

    return true;
}

} 
ReceiveResult listen_for_update(uint32_t listen_window_ms, Bank currently_active_bank) {
    uint32_t start_tick = sys::millis();

    uint8_t payload[ota::MAX_PAYLOAD];
    uint16_t payload_len = 0;
    ota::PacketType type;
    uint16_t seq = 0;

    bool got_start = false;
    while ((sys::millis() - start_tick) < listen_window_ms) {
        led::toggle(); // heartbeat: visible proof the bootloader is alive and listening
        if (receive_packet(payload, &payload_len, &type, &seq, 100)) {
            if (type == ota::PacketType::START) {
                got_start = true;
                break;
            }
        }
    }
    if (!got_start) return ReceiveResult::NO_UPDATE_ATTEMPTED;

    uint32_t total_len;
    memcpy(&total_len, payload, sizeof(total_len));

    Bank target_bank = other_bank(currently_active_bank);
    if (total_len == 0 || total_len > bank_size(target_bank)) {
        send_ack_or_nack(false, seq);
        return ReceiveResult::TRANSFER_ABORTED;
    }
    if (!erase_bank(target_bank)) {
        send_ack_or_nack(false, seq);
        return ReceiveResult::TRANSFER_ABORTED;
    }
    send_ack_or_nack(true, seq);

    uint32_t write_offset = 0;
    uint8_t consecutive_failures = 0;

    while (write_offset < total_len) {
        if (!receive_packet(payload, &payload_len, &type, &seq, 2000)) {
            send_ack_or_nack(false, seq);
            if (++consecutive_failures > ota::MAX_RETRIES) return ReceiveResult::TRANSFER_ABORTED;
            continue;
        }
        if (type != ota::PacketType::DATA) {
            send_ack_or_nack(false, seq);
            continue;
        }

        if (!flash_write(bank_start(target_bank) + write_offset, payload, payload_len)) {
            send_ack_or_nack(false, seq);
            if (++consecutive_failures > ota::MAX_RETRIES) return ReceiveResult::TRANSFER_ABORTED;
            continue;
        }

        consecutive_failures = 0;
        write_offset += payload_len;
        send_ack_or_nack(true, seq);
    }

    if (!receive_packet(payload, &payload_len, &type, &seq, 2000) ||
        type != ota::PacketType::END || payload_len != ED25519_SIGNATURE_SIZE) {
        send_ack_or_nack(false, seq);
        return ReceiveResult::TRANSFER_ABORTED;
    }

    const uint8_t* written_image = reinterpret_cast<const uint8_t*>(bank_start(target_bank));
    bool sig_ok = verify_firmware_signature(payload, written_image, total_len);

    if (!sig_ok) {
        send_ack_or_nack(false, seq);
        return ReceiveResult::SIGNATURE_INVALID;
    }

    send_ack_or_nack(true, seq);
    mark_update_pending(target_bank);
    return ReceiveResult::SUCCESS;
}

} // namespace boot
