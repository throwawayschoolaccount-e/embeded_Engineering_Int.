// main.cpp - STM32F103 bare-metal bootloader (no HAL/CubeMX).
// Boot decision -> listen for update over UART2 -> jump to application.
#include "regmap_f103.h"
#include "system_init.h"
#include "uart2.h"
#include "flash_driver.h"
#include "rollback.h"
#include "uart_receiver.h"
#include "led.h"

namespace {

constexpr uint32_t UPDATE_LISTEN_WINDOW_MS = 20000; // 20s for easier testing; shorten later
constexpr uint32_t UART_BAUD = 115200;

[[noreturn]] void jump_to_application(uint32_t bank_addr) {
    typedef void (*ResetHandler)(void);

    uint32_t app_sp    = *reinterpret_cast<uint32_t*>(bank_addr);
    uint32_t app_reset = *reinterpret_cast<uint32_t*>(bank_addr + 4);

    // Leave peripherals in a clean state for the application.
    uart2::deinit();
    SYSTICK->CTRL = 0;

    *SCB_VTOR = bank_addr;
    __asm__ volatile("msr msp, %0" : : "r"(app_sp));

    reinterpret_cast<ResetHandler>(app_reset)();

    while (true) { /* unreachable */ }
}

} // namespace

int main() {
    sys::init();
    uart2::init(UART_BAUD);
    led::init();

    // Startup heartbeat: 3 quick blinks proves the bootloader itself
    // started running (as opposed to hanging in Reset_Handler, or never
    // getting flashed correctly).
    for (int i = 0; i < 6; ++i) {
        led::toggle();
        sys::delay_ms(150);
    }
    led::off();

    boot::BootFlags flags = boot::read_flags();

    // --- Boot decision: check update flag -------------------------------
    if (flags.pending_bank != boot::Bank::NONE) {
        if (boot::should_rollback(flags)) {
            flags.pending_bank = boot::Bank::NONE;
            flags.boot_attempts = 0;
            boot::write_flags(flags);
        } else {
            flags = boot::record_boot_attempt(flags);
            led::on(); // solid on = bootloader reached its jump decision
            jump_to_application(boot::bank_start(flags.pending_bank));
        }
    }

    // No pending update to try: give the host a short window to push a
    // new firmware image before booting normally.
    boot::listen_for_update(UPDATE_LISTEN_WINDOW_MS, flags.active_bank);

    flags = boot::read_flags();

    uint32_t boot_target = (flags.pending_bank != boot::Bank::NONE)
                               ? boot::bank_start(flags.pending_bank)
                               : boot::bank_start(flags.active_bank);

    if (flags.pending_bank != boot::Bank::NONE) {
        flags = boot::record_boot_attempt(flags);
    }

    led::on(); // solid on = bootloader reached its jump decision
    jump_to_application(boot_target);
}
