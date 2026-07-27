// uart2.h
// Raw USART2 driver: PA2 = TX, PA3 = RX (the Nucleo's D1/D0 Arduino header
// pins). Replaces the HAL_UART_* calls from the earlier HAL-based version.
#pragma once
#include <cstdint>
#include <cstddef>

namespace uart2 {

void init(uint32_t baud);

void send_byte(uint8_t b);
void send_buffer(const uint8_t* data, size_t len);

// Returns false if the byte/buffer didn't fully arrive within timeout_ms.
bool receive_byte(uint8_t* out, uint32_t timeout_ms);
bool receive_buffer(uint8_t* out, size_t len, uint32_t timeout_ms);

// Disables USART2 and its clock used right before jumping to the
// application so it starts from a clean peripheral state.
void deinit();

} 
