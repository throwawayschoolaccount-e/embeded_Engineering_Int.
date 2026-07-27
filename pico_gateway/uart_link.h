// uart_link.h
// Sends a firmware image to the STM32 bootloader over UART using the
// ota::PacketType framing defined in ota_protocol.h, with per-packet
// ACK/NACK and retry.
#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include "ota_protocol.h"

namespace gateway {

enum class UartResult {
    OK,
    TIMEOUT,
    REJECTED,       // receiver NACKed after exhausting retries
    LINK_ERROR,
};

// Distinguishes "nothing came back at all" from "the STM32 explicitly
// NACKed" - these look identical if you only check a bool, which was a
// real bug: it made every timeout get reported as if the STM32 had
// responded.
enum class AckStatus {
    ACK,
    NACK,
    TIMEOUT,
};

// Called periodically with (bytes_sent, bytes_total) so the HTTP status
// endpoint can report progress to the dashboard.
using ProgressCallback = std::function<void(uint32_t sent, uint32_t total)>;

class UartLink {
public:
    void init(uint32_t baud_rate);

    // Streams `data` (length `len`) to the STM32, framed as START, then
    // DATA packets, then END (with the Ed25519 signature appended).
    // Returns once the whole transfer is acknowledged or has failed.
    UartResult send_firmware(const uint8_t* data,
                              uint32_t len,
                              const uint8_t signature[64],
                              const ProgressCallback& on_progress);

private:
    AckStatus send_packet_and_wait_ack(ota::PacketType type,
                                        uint16_t seq,
                                        const uint8_t* payload,
                                        uint16_t payload_len);

    AckStatus read_ack_or_nack(uint32_t timeout_ms);

    uint16_t next_seq_ = 0;
};

} // namespace gateway
