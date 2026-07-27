// http_server.h
// Minimal HTTP server on the Pico 2W, built on lwIP's raw TCP API (no RTOS
// needed). Handles:
//   POST /update   - body is the signed firmware binary, header
//                     "X-Signature" carries the 64-byte Ed25519 signature
//                     as 128 hex characters.
//   GET  /status    - returns JSON with current transfer progress, for the
//                     "Web Dashboard" box in your diagram.
#pragma once

#include <cstdint>
#include "uart_link.h"

namespace gateway {

struct TransferStatus {
    bool     in_progress = false;
    uint32_t bytes_sent  = 0;
    uint32_t bytes_total = 0;
    bool     last_result_ok = true;
    char     last_message[64] = "idle";
};

// Starts listening on port 80. `link` is used to relay accepted firmware
// to the STM32 over UART.
void http_server_start(UartLink* link, TransferStatus* status);

} // namespace gateway
