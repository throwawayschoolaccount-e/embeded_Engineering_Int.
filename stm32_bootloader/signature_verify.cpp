// signature_verify.cpp
#include "signature_verify.h"

// From the vendored orlp/ed25519 library (see signature_verify.h for the
// source). Its C API looks like:
//   int ed25519_verify(const unsigned char *signature,
//                       const unsigned char *message, size_t message_len,
//                       const unsigned char *public_key);
extern "C" {
#include "ed25519.h" // found via -I third_party/ed25519/src in the Makefile
}

namespace boot {
    //place holder public key
const uint8_t OTA_PUBLIC_KEY[ED25519_PUBLIC_KEY_SIZE] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

bool verify_firmware_signature(const uint8_t signature[ED25519_SIGNATURE_SIZE],
                                const uint8_t* message,
                                size_t message_len) {
    return ed25519_verify(signature, message, message_len, OTA_PUBLIC_KEY) != 0;
}

} 
