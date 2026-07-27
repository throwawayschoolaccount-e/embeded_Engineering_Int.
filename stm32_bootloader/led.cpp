// led.cpp
#include "led.h"
#include "regmap_f103.h"

namespace led {

void init() {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN; // harmless if uart2::init() already set this
    // PA5, output push-pull, 10 MHz (MODE=01, CNF=00)
    GPIOA->CRL &= ~(0xFu << 20);
    GPIOA->CRL |=  (0x1u << 20);
}

void on()  { GPIOA->BSRR = (1u << 5); }        // BSRR set bits: pin 5
void off() { GPIOA->BSRR = (1u << (5 + 16)); } // BSRR reset bits: pin 5 + 16
void toggle() { GPIOA->ODR ^= (1u << 5); }

} 
