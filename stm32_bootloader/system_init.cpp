// system_init.cpp
#include "system_init.h"
#include "regmap_f103.h"

namespace {
volatile uint32_t g_millis = 0;
}

extern "C" void SysTick_Handler(void) {
    g_millis++;
}

namespace sys {

void init() {
    SYSTICK->LOAD = (SYSCLK_HZ / 1000u) - 1u; // 1ms period
    SYSTICK->VAL  = 0;
    SYSTICK->CTRL = SYSTICK_CTRL_CLKSOURCE | SYSTICK_CTRL_TICKINT | SYSTICK_CTRL_ENABLE;
}

uint32_t millis() {
    return g_millis;
}

void delay_ms(uint32_t ms) {
    uint32_t start = millis();
    while ((millis() - start) < ms) {
        __asm__ volatile("nop");
    }
}

} 
