// uart_link.cpp
#include "uart_link.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "pico/time.h"
#include <cstring>

// GPIO4 (TX) -> STM32 RX/D0 (USART2 RX)
// GPIO5 (RX) -> STM32 TX/D1 (USART2 TX)


#define OTA_UART        uart1
#define OTA_UART_TX_PIN 4
#define OTA_UART_RX_PIN 5

namespace gateway {

void UartLink::init(uint32_t baud_rate) {
    uart_init(OTA_UART, baud_rate);
    gpio_set_function(OTA_UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(OTA_UART_RX_PIN, GPIO_FUNC_UART);
    uart_set_hw_flow(OTA_UART, false, false);
    uart_set_format(OTA_UART, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(OTA_UART, true);
    next_seq_ = 0;
}

static void write_bytes(const uint8_t* data, size_t len) {
    uart_write_blocking(OTA_UART, data, len);
}

AckStatus UartLink::read_ack_or_nack(uint32_t timeout_ms) {
    // Expect a full minimal packet: header (6) + 0-byte payload + crc (4) = 10 bytes.
    uint8_t buf[ota::HEADER_SIZE + ota::CRC_SIZE];
    size_t received = 0;
    absolute_time_t deadline = make_timeout_time_ms(timeout_ms);

    while (received < sizeof(buf)) {
        if (time_reached(deadline)) return AckStatus::TIMEOUT;
        if (uart_is_readable(OTA_UART)) {
            buf[received++] = uart_getc(OTA_UART);
        }
    }

    auto* hdr = reinterpret_cast<ota::PacketHeader*>(buf);
    if (hdr->sync != ota::SYNC_BYTE) return AckStatus::TIMEOUT; // garbage, not a real response

    uint32_t crc_received;
    std::memcpy(&crc_received, buf + ota::HEADER_SIZE, sizeof(crc_received));
    uint32_t crc_calc = ota::Crc32::compute(buf, ota::HEADER_SIZE);
    if (crc_received != crc_calc) return AckStatus::TIMEOUT; // garbled, not a real response

    return (hdr->type == static_cast<uint8_t>(ota::PacketType::ACK)) ? AckStatus::ACK : AckStatus::NACK;
}

AckStatus UartLink::send_packet_and_wait_ack(ota::PacketType type,
                                              uint16_t seq,
                                              const uint8_t* payload,
                                              uint16_t payload_len) {
    uint8_t frame[ota::MAX_PACKET_SIZE];
    ota::PacketHeader hdr{ota::SYNC_BYTE, static_cast<uint8_t>(type), seq, payload_len};

    std::memcpy(frame, &hdr, ota::HEADER_SIZE);
    if (payload_len > 0) {
        std::memcpy(frame + ota::HEADER_SIZE, payload, payload_len);
    }
    uint32_t crc = ota::Crc32::compute(frame, ota::HEADER_SIZE + payload_len);
    std::memcpy(frame + ota::HEADER_SIZE + payload_len, &crc, sizeof(crc));

    size_t total_len = ota::HEADER_SIZE + payload_len + ota::CRC_SIZE;

    AckStatus last_status = AckStatus::TIMEOUT;
    for (uint8_t attempt = 0; attempt < ota::MAX_RETRIES; ++attempt) {
        write_bytes(frame, total_len);
        AckStatus status = read_ack_or_nack(ota::ACK_TIMEOUT_MS);
        if (status == AckStatus::ACK) {
            return AckStatus::ACK;
        }
        if (status == AckStatus::NACK) {
            last_status = AckStatus::NACK; // real communication happened, even though rejected
        }
        // NACK or timeout: retry the same sequence number.
    }
    return last_status;
}

UartResult UartLink::send_firmware(const uint8_t* data,
                                    uint32_t len,
                                    const uint8_t signature[64],
                                    const ProgressCallback& on_progress) {
    next_seq_ = 0;

    // START packet: 4-byte little-endian total length.
    uint8_t start_payload[4];
    start_payload[0] = static_cast<uint8_t>(len & 0xFF);
    start_payload[1] = static_cast<uint8_t>((len >> 8) & 0xFF);
    start_payload[2] = static_cast<uint8_t>((len >> 16) & 0xFF);
    start_payload[3] = static_cast<uint8_t>((len >> 24) & 0xFF);

    AckStatus status = send_packet_and_wait_ack(ota::PacketType::START, next_seq_++, start_payload, sizeof(start_payload));
    if (status == AckStatus::TIMEOUT) return UartResult::TIMEOUT;
    if (status == AckStatus::NACK) return UartResult::REJECTED;

    uint32_t sent = 0;
    while (sent < len) {
        uint16_t chunk = static_cast<uint16_t>(
            (len - sent) > ota::MAX_PAYLOAD ? ota::MAX_PAYLOAD : (len - sent));

        status = send_packet_and_wait_ack(ota::PacketType::DATA, next_seq_++, data + sent, chunk);
        if (status == AckStatus::TIMEOUT) return UartResult::TIMEOUT;
        if (status == AckStatus::NACK) return UartResult::REJECTED;

        sent += chunk;
        if (on_progress) on_progress(sent, len);
    }

    // END packet carries the Ed25519 signature of the whole image; the
    // bootloader verifies it before marking the update valid.
    status = send_packet_and_wait_ack(ota::PacketType::END, next_seq_++, signature, 64);
    if (status == AckStatus::TIMEOUT) return UartResult::TIMEOUT;
    if (status == AckStatus::NACK) return UartResult::REJECTED;

    return UartResult::OK;
}

} 
