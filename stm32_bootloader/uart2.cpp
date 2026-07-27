// uart2.cpp
#include "uart2.h"
#include "regmap_f103.h"
#include "system_init.h"

namespace uart2 {

void init(uint32_t baud) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    // PA2 = USART2_TX: alternate function push-pull, 50 MHz (MODE=11, CNF=10)
    GPIOA->CRL &= ~(0xFu << 8);
    GPIOA->CRL |=  (0xBu << 8);

    // PA3 = USART2_RX: input floating (MODE=00, CNF=01)
    GPIOA->CRL &= ~(0xFu << 12);
    GPIOA->CRL |=  (0x4u << 12);

    // BRR = fPCLK1 / (16 * baud), computed in fixed point (x100) to avoid
    // needing float/soft-float support in a bootloader.
    uint32_t usartdiv_x100 = (sys::SYSCLK_HZ * 100u) / (16u * baud);
    uint32_t mantissa = usartdiv_x100 / 100u;
    uint32_t fraction = ((usartdiv_x100 - mantissa * 100u) * 16u + 50u) / 100u;
    if (fraction > 15u) {
        mantissa += 1u;
        fraction -= 16u;
    }
    USART2_REG->BRR = (mantissa << 4) | (fraction & 0xFu);

    USART2_REG->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

void send_byte(uint8_t b) {
    while (!(USART2_REG->SR & USART_SR_TXE)) {}
    USART2_REG->DR = b;
}

void send_buffer(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) send_byte(data[i]);
}

bool receive_byte(uint8_t* out, uint32_t timeout_ms) {
    uint32_t start = sys::millis();
    while (!(USART2_REG->SR & USART_SR_RXNE)) {
        if ((sys::millis() - start) >= timeout_ms) return false;
    }
    *out = static_cast<uint8_t>(USART2_REG->DR & 0xFFu);
    return true;
}

bool receive_buffer(uint8_t* out, size_t len, uint32_t timeout_ms) {
    uint32_t start = sys::millis();
    for (size_t i = 0; i < len; ++i) {
        uint32_t elapsed = sys::millis() - start;
        uint32_t remaining = (elapsed >= timeout_ms) ? 0u : (timeout_ms - elapsed);
        if (!receive_byte(&out[i], remaining)) return false;
    }
    return true;
}

void deinit() {
    USART2_REG->CR1 = 0;
    RCC->APB1ENR &= ~RCC_APB1ENR_USART2EN;
}

} // namespace uart2
