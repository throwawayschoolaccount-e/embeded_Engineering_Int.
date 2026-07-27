// system_init.h
// Minimal system setup: we deliberately leave the clock at its default
// reset state (HSI, 8 MHz, no PLL) to avoid clock-config bugs - a
// bootloader is exactly the wrong place to debug a botched PLL setup.
// SysTick gives us a millisecond timer for UART timeouts.
#pragma once
#include <cstdint>

namespace sys {

constexpr uint32_t SYSCLK_HZ = 8000000u; // default HSI, no PLL

void init();                 // configure SysTick for 1ms ticks
uint32_t millis();           // milliseconds since boot
void delay_ms(uint32_t ms);

} 
