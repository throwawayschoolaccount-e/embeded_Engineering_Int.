// signature_verify.h
//
// This wraps a third-party, audited Ed25519 implementation rather than
// reimplementing the curve arithmetic here - hand-rolled crypto primitives
// are a common source of subtle, exploitable bugs. Recommended library:
//   https://github.com/orlp/ed25519  (MIT license, no heap allocation,
//   plain C, ~700 lines, designed for exactly this embedded use case)
//
// Steps to wire it up:
//   1. Vendor ed25519.c/ed25519.h (and its sha512.c) into this project,
//      e.g. under stm32_bootloader/third_party/ed25519/.
//   2. Add it to your build (Makefile / CubeIDE project / CMake).
//   3. Generate a keypair on the host machine (this matches the
//      "Ed25519 Key Signing" box in your diagram) and paste the 32-byte
//      public key into ota_public_key.cpp - NEVER put the private key on
//      the device.
#pragma once

#include <cstdint>
#include <cstddef>

namespace boot {

constexpr size_t ED25519_SIGNATURE_SIZE = 64;
constexpr size_t ED25519_PUBLIC_KEY_SIZE = 32;

// The public key baked into the bootloader at build time.
extern const uint8_t OTA_PUBLIC_KEY[ED25519_PUBLIC_KEY_SIZE];

// Verifies `signature` over `message` (the full firmware image as written
// into the target bank) against OTA_PUBLIC_KEY.
bool verify_firmware_signature(const uint8_t signature[ED25519_SIGNATURE_SIZE],
                                const uint8_t* message,
                                size_t message_len);

} // namespace boot
