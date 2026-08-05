[ota_system_explained.md](https://github.com/user-attachments/files/30750844/ota_system_explained.md)# Secure OTA Firmware Update System: How It Works

A walkthrough of every piece of code in this project, what it does, and how to know it's actually working when you run it.

---

## 1. The big picture

```
Host (signs firmware)  --WiFi/HTTPS-->  Pico 2W (gateway)  --UART-->  STM32 (bootloader)
        |                                      |                            |
   Ed25519 private key                  relays bytes,              verifies signature,
   never leaves host                    speaks HTTP + UART         flashes bank B,
                                                                     decides what to boot
```

Three independent pieces of code, each with one job:

| Component | Job | Language/toolchain |
|---|---|---|
| Host script (not built yet) | Sign firmware with Ed25519 | Python |
| `pico_gateway/` | Receive firmware over WiFi, relay over UART | C++, Pico SDK |
| `stm32_bootloader/` | Verify signature, flash it, decide what to boot | C++, bare registers (no HAL) |

---

## 2. The shared protocol (`ota_protocol.h`)

Both boards speak the same framing over the wire, byte for byte:

```
[SYNC 1B][TYPE 1B][SEQ 2B][LEN 2B][PAYLOAD up to 256B][CRC32 4B]
```

- **SYNC** (`0xA5`) lets a receiver resync if bytes get garbled.
- **TYPE** is one of `START` (announces total firmware size), `DATA` (a chunk of firmware), `END` (carries the 64-byte Ed25519 signature), `ACK`, `NACK`.
- **CRC32** catches transmission errors, a checksum, not a security measure. It only proves the bytes weren't corrupted in transit, not that they came from someone you trust. That's what the signature is for.
- Every `START`/`DATA`/`END` packet must be acknowledged before the sender moves to the next one, with up to 5 retries.

This file is identical on both sides, copy-pasted, not shared at build time, so a mismatch here would be a real bug, not just a config issue.

---

## 3. The Pico gateway (`pico_gateway/`)

### `main.cpp`
Boots the board, connects to WiFi (`cyw43_arch_wifi_connect_timeout_ms`), starts the UART link at 115200 baud, and starts the HTTP server. Also runs a **temporary test probe** every 2 seconds that sends a dummy (all-zero) signed packet through the real protocol, this exists purely so you can verify the UART link works before you have a real signing pipeline. It's marked for removal once real `POST /update` testing begins.

### `http_server.cpp`
A minimal HTTP server built on lwIP's raw (callback-based) API, no RTOS, no blocking `accept()` loop. It handles exactly two routes:
- `POST /update`, reads `Content-Length` and an `X-Signature` header (128 hex characters = 64 bytes), buffers the whole firmware body, then hands it to the UART link.
- `GET /status`, returns JSON with transfer progress, for a dashboard.

### `uart_link.cpp`
Packetizes a firmware buffer into `START` → many `DATA` → `END` packets, waiting for an ACK after each one. Distinguishes a genuine timeout (`AckStatus::TIMEOUT`, nothing came back) from an explicit rejection (`AckStatus::NACK`, the STM32 responded but rejected it), **this distinction was a real bug we found and fixed** during testing; originally both cases were reported identically, which gave a false "it's working" message when it was actually just timing out.

---

## 4. The STM32 bootloader (`stm32_bootloader/`)

This is deliberately **bare-metal, no HAL, no CubeMX**. Every peripheral is touched through direct register structs (`regmap_f103.h`), the same style proven by your blink test. This was a design change made mid-project once we discovered the real chip is an STM32F103 (128KB flash) rather than the STM32F4 originally assumed, the whole memory map and flash driver were rebuilt around that constraint.

### `regmap_f103.h`
Struct-over-address definitions for RCC (clocks), GPIOA, USART2, the flash controller (FPEC), and SysTick. No CMSIS, no vendor headers.

### `system_init.cpp`
Configures SysTick for a 1ms interrupt-driven tick, giving `sys::millis()` and `sys::delay_ms()`. The clock is deliberately left at its default 8MHz HSI (no PLL), a bootloader is the wrong place to debug a botched clock configuration.

### `uart2.cpp`
Hand-rolled USART2 driver on PA2 (TX) / PA3 (RX), the Nucleo's `TX/D1` / `RX/D0` header pins. Baud rate is computed in fixed-point arithmetic (avoiding the need for float/soft-float support). Blocking send, and receive-with-timeout using the millisecond timer above.

### `flash_driver.cpp`
Raw FPEC register sequences: unlock (write two magic key words), erase-by-page (STM32F1 erases in 1KB pages, not sectors like F4/F7), and program-by-halfword (F1's native write granularity).

### Flash memory map (`flash_driver.h`)
128KB total, split as:

| Region | Size | Purpose |
|---|---|---|
| Bootloader | 24 KB | This code + the Ed25519 library |
| Bank A | 51 KB | "Running firmware" |
| Bank B | 51 KB | "New firmware target" |
| Flags sector | 2 KB | Boot state (which bank is active/pending) |

This is genuinely tight, the Ed25519+SHA-512 library alone is ~35KB uncompiled, trimmed to fit via `-ffunction-sections -fdata-sections` + `--gc-sections`, which strips out the signing-only code paths we don't need on-device (verification only).

### `rollback.cpp`
The crash-safety core. A small struct (`BootFlags`), active bank, pending bank, boot attempt count, confirmed flag, lives in its own flash sector with a CRC32 over the whole struct. If power is lost mid-write, the CRC simply won't match on next boot, and the bootloader falls back to defaults (bank A, no pending update) rather than trusting a half-written state.

Key functions:
- `mark_update_pending(bank)`, called after a signature verifies successfully.
- `record_boot_attempt()`, increments a counter every time the bootloader tries booting a pending image.
- `confirm_current_firmware()`, **must be called by the application itself** once it's confirmed healthy. Without this call, `should_rollback()` will eventually trip after `MAX_BOOT_ATTEMPTS` (3) resets, and the bootloader reverts to the last known-good bank.

### `signature_verify.cpp`
Wraps the vendored [`orlp/ed25519`](https://github.com/orlp/ed25519) library rather than reimplementing curve arithmetic, Ed25519 math is exactly the kind of thing where a hand-rolled implementation becomes a security hole. Only `verify.c`, `sha512.c`, `ge.c`, `fe.c`, `sc.c` are compiled in; the signing/keypair files aren't needed on-device.

### `uart_receiver.cpp`
The actual OTA protocol state machine: waits for `START` (reads total firmware size, erases the *inactive* bank), then `DATA` packets (writes each chunk, ACKs or NACKs based on CRC), then `END` (the Ed25519 signature), only calling `mark_update_pending()` if the signature actually verifies against the image as written in flash. Also pulses LD2 on/off for every byte-perfect packet received, a debug aid added specifically because this bootloader has no serial console (USART2 is dedicated to the OTA link).

### `main.cpp`
The boot decision logic:
1. Read boot flags.
2. If a pending bank exists and hasn't exceeded its attempt budget → jump to it (with the attempt counter incremented).
3. If it *has* exceeded the budget → clear the pending flag (rollback) and fall through to the active bank.
4. Otherwise, listen for a new update for a window of time (currently widened to 20s for easier testing; production would use something shorter, like the original 3s).
5. Jump to whatever bank was decided on, via direct vector-table relocation (`SCB_VTOR`) and manual stack pointer / reset vector setup, no HAL abstraction for this either.

---

## 5. What's been verified so far (and how)

This is worth being explicit about, since "the code compiles" and "the code works on real hardware" are different claims:

| Claim | How it was actually verified |
|---|---|
| Blink test toolchain works | Built with `arm-none-eabi-gcc`, disassembled and confirmed the vector table's stack pointer (`0x20005000`) and reset vector by hand |
| Bootloader compiles and fits | Built with zero warnings, 21,288 of 24,576 bootloader-budget bytes used |
| Bootloader boot logic runs on real hardware | LD2 sequence observed matches the code exactly: 3 startup blinks → listen-window heartbeat → solid on at boot decision |
| Pico gateway compiles against real SDK | Built against actual `raspberrypi/pico-sdk` (not assumed), zero warnings in our own files |
| Pico connects to real WiFi | Confirmed via serial output: `WiFi connected` |
| UART wiring is physically correct | Confirmed pin-by-pin from your photos: GP4→RX/D0, GP5←TX/D1, GND↔GND |
| STM32 actually receives Pico's test packets | **Not yet confirmed**, this is the open thread as of this document |

That last row matters: everything above it is real, hardware-verified evidence. The UART communication test is the one piece still pending a conclusive logic-analyzer capture.

---

## 6. Expected output at each stage (success criteria)

Use this as a checklist. Each row is something you should be able to point at and say "yes, that happened" or "no, it didn't."

| Stage | What you do | What success looks like | What failure looks like |
|---|---|---|---|
| Blink test | Flash `blink_test.hex` | LD2 blinks steadily, ~1x/second | No light, or solid on/off with no blinking |
| Bootloader flash | Flash `ota_bootloader.hex`, press reset | 3 quick blinks → blinking heartbeat for the listen window → solid on | No blinks at all (bad flash or reset not working) |
| Pico WiFi | Flash gateway `.uf2`, watch serial | `connecting to WiFi...` then `WiFi connected` then `HTTP server listening on port 80` | `WiFi connect failed` (check credentials), or nothing prints at all (check `stdio_usb_connected()` wait, or that you opened the terminal before/during boot) |
| UART wiring | Reset STM32, watch both boards during the listen window | STM32's LD2 **pulses distinctly** (not just the ambient heartbeat) in response to a Pico probe; Pico prints `STM32 responded (NACK on bad signature - this is expected and means wiring works!)` | Pico loops printing `no response from STM32` for the entire window, and STM32's LD2 never pulses beyond its own heartbeat |
| Real signed update (not built yet) | POST real signed firmware to Pico | STM32 LD2 pulses through the whole transfer, `mark_update_pending()` succeeds, next boot tries bank B, application calls `confirm_current_firmware()`, bank B becomes the new active bank | Signature check fails (NACK on END packet) if the public key doesn't match the private key used to sign, or bank never gets confirmed and rolls back after 3 attempts |

---

## 7. Known gaps before this is a real, working system

Being direct about what's still incomplete:

1. **The Ed25519 public key is still all zeros** in `signature_verify.cpp`, a real keypair needs generating, with only the public half going on-device.
2. **No real application image exists yet**, bank A is blank flash. The bootloader has nothing valid to boot into, which is why it currently ends up stuck in a fault handler after its listen window closes.
3. **The UART communication test is unresolved**, the wiring looks correct from photos and the diagram, but no logic-analyzer capture has yet confirmed bytes actually arriving at the STM32's RX pin during a probe.
4. **The listen window is currently 20 seconds for debugging**, production use would want this much shorter (the original design was 3 seconds) so the device isn't sitting open to UART input for that long on every boot.
5. **WiFi credentials are hardcoded** in `main.cpp` rather than kept out of version control.

---

## 8. Conclusion

The architecture is sound and most of it is hardware-verified, not just "should work in theory": both toolchains build clean, the bootloader's control flow has been observed matching its source code exactly via LED behavior, and the Pico's WiFi stack is confirmed live. The one genuinely open question is whether the physical UART link is actually passing bytes. Everything needed to answer that (both firmwares built and flashed, a repeating test probe, and a documented wiring diagram to check against) is in place. The next concrete action is a logic-analyzer capture during a probe attempt, which will conclusively point to either the physical connection or the STM32-side receive code as the remaining gap.
