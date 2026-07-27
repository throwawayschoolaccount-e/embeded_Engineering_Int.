// main.cpp - Pico 2W "WiFi Gateway" from the diagram.
// Connects to WiFi, exposes POST /update and GET /status, and relays
// accepted firmware to the STM32 bootloader over UART.
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "uart_link.h"
#include "http_server.h"
#include <cstdio>

// TODO: move these to a gitignored wifi_credentials.h before committing.
#define WIFI_SSID "11111"
#define WIFI_PASSWORD "11111111"

int main() {
    stdio_init_all();

    // Wait (briefly) for a terminal to actually open the USB CDC port
    // before printing anything - otherwise the first prints can be lost
    // if you attach picocom/etc a moment too late.
    for (int i = 0; i < 50 && !stdio_usb_connected(); ++i) {
        sleep_ms(100);
    }

    printf("\n--- ota_gateway starting ---\n");

    if (cyw43_arch_init()) {
        printf("failed to init cyw43\n");
        return -1;
    }

    cyw43_arch_enable_sta_mode();

    printf("connecting to WiFi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD,
                                            CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("WiFi connect failed\n");
        return -1;
    }
    printf("WiFi connected\n");

    static gateway::UartLink uart_link;
    uart_link.init(115200);

    static gateway::TransferStatus status;
    gateway::http_server_start(&uart_link, &status);

    printf("HTTP server listening on port 80\n");

    // --- TEMPORARY WIRING TEST -------------------------------------------
    // Sends a small dummy "firmware" through the real protocol every few
    // seconds, purely so you can confirm the UART link to the STM32 is
    // actually working before you have real signed firmware to send.
    // The STM32 will ACK the START/DATA packets and its LD2 will pulse for
    // each one it receives - then it'll NACK the END packet, since this
    // dummy signature is all zeros and won't verify. That NACK is expected
    // and fine: it proves the whole transport works even though this isn't
    // a real update. Remove this block once you're testing with real
    // signed firmware via POST /update instead.
    static uint8_t test_payload[16] = {0};
    static uint8_t test_signature[64] = {0};
    absolute_time_t next_probe = get_absolute_time();
    // --- end temporary wiring test setup ---

    // cyw43_arch's lwIP integration runs the network stack from a
    // background context, so the main loop just needs to stay alive.
    while (true) {
        cyw43_arch_poll();

        // --- TEMPORARY WIRING TEST: fire a probe every 2 seconds ---
        if (absolute_time_diff_us(get_absolute_time(), next_probe) <= 0) {
            printf("[probe] sending test packet to STM32...\n");
            gateway::UartResult result = uart_link.send_firmware(
                test_payload, sizeof(test_payload), test_signature, nullptr);
            switch (result) {
                case gateway::UartResult::OK:
                    printf("[probe] STM32 accepted it (unexpected - check signature logic)\n");
                    break;
                case gateway::UartResult::REJECTED:
                    printf("[probe] STM32 responded (NACK on bad signature - this is expected and means wiring works!)\n");
                    break;
                case gateway::UartResult::TIMEOUT:
                    printf("[probe] no response from STM32 - check wiring/baud/reset state\n");
                    break;
                default:
                    printf("[probe] link error\n");
                    break;
            }
            next_probe = make_timeout_time_ms(2000);
        }
        // --- end temporary wiring test ---

        sleep_ms(10);
    }
}
