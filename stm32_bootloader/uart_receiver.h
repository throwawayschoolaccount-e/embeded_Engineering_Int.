// uart_receiver.h
// STM32 side of the custom binary protocol: receives START/DATA/END
// packets, writes firmware into the inactive flash bank, and ACKs/NACKs
// each packet. Mirrors gateway::UartLink on the Pico side.
#pragma once

#include <cstdint>
#include "flash_driver.h"

namespace boot {

enum class ReceiveResult {
    NO_UPDATE_ATTEMPTED,  // no START packet seen within the listen window
    SUCCESS,              // full image received and signature verified
    SIGNATURE_INVALID,
    TRANSFER_ABORTED,     // too many bad packets, or receiver ran out of space
};

// Listens on UART for up to `listen_window_ms`. If a START packet arrives,
// receives the whole image into the bank NOT currently marked active
// (`flags.active_bank`), verifies its signature, and - only if valid -
// calls mark_update_pending() so the boot decision logic will try it next.
ReceiveResult listen_for_update(uint32_t listen_window_ms, Bank currently_active_bank);

} // namespace boot
