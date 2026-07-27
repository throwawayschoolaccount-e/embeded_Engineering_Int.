// led.h
// Nucleo-F103RB's onboard LD2 is wired to PA5 - same pin the blink test
// used. Purely a debug aid: the bootloader has no serial console (USART2
// is dedicated to the OTA link), so this is how you can visually confirm
// what the bootloader is doing without a logic analyzer.
#pragma once

namespace led {

void init();
void on();
void off();
void toggle();

} // namespace led
