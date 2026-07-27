// ota_protocol.h
// Shared packet format for the UART link between the Pico 2W gateway
// and the STM32 bootloader. Include this file identically on both sides.
#pragma once

#include <cstdint>
#include <cstddef>

namespace ota {

// ---- Wire format -----------------------------------------------------
// [ SYNC ][ TYPE ][ SEQ_LO ][ SEQ_HI ][ LEN_LO ][ LEN_HI ][ PAYLOAD... ][ CRC32 (4 bytes, little endian) ]
//
// - SYNC is always 0xA5, used by the receiver to resync after a framing error.
// - CRC32 is computed over TYPE + SEQ + LEN + PAYLOAD (i.e. everything after SYNC,
//   before the CRC field itself).
// - Every DATA/START/END packet must be ACKed or NACKed before the sender moves on.
// - seq numbers increment per packet and let the receiver detect duplicates
//   (e.g. if an ACK is lost and the sender retransmits).

constexpr uint8_t  SYNC_BYTE      = 0xA5;
constexpr uint16_t MAX_PAYLOAD    = 256;     // bytes of firmware data per packet
constexpr uint8_t  MAX_RETRIES    = 5;
constexpr uint32_t ACK_TIMEOUT_MS = 500;

enum class PacketType : uint8_t {
    START = 0x01,  // payload = total firmware size (4 bytes, little endian)
    DATA  = 0x02,  // payload = raw firmware bytes
    END   = 0x03,  // payload = 64-byte Ed25519 signature of the full image
    ACK   = 0x04,  // payload = empty
    NACK  = 0x05,  // payload = empty (receiver did not accept the last packet)
};

#pragma pack(push, 1)
struct PacketHeader {
    uint8_t  sync;
    uint8_t  type;     // PacketType
    uint16_t seq;
    uint16_t length;   // payload length in bytes, 0..MAX_PAYLOAD
};
#pragma pack(pop)

constexpr size_t HEADER_SIZE = sizeof(PacketHeader); // 6 bytes
constexpr size_t CRC_SIZE    = sizeof(uint32_t);     // 4 bytes
constexpr size_t MAX_PACKET_SIZE = HEADER_SIZE + MAX_PAYLOAD + CRC_SIZE;

// ---- CRC32 (standard IEEE 802.3 polynomial, table-based) --------------
// Small, dependency-free implementation so both firmware images can embed
// it without pulling in a compression/crypto library just for framing.
class Crc32 {
public:
    static uint32_t compute(const uint8_t* data, size_t len) {
        uint32_t crc = 0xFFFFFFFFu;
        for (size_t i = 0; i < len; ++i) {
            crc = (crc >> 8) ^ table()[(crc ^ data[i]) & 0xFF];
        }
        return crc ^ 0xFFFFFFFFu;
    }

private:
    static const uint32_t* table() {
        static uint32_t t[256];
        static bool init = false;
        if (!init) {
            for (uint32_t i = 0; i < 256; ++i) {
                uint32_t c = i;
                for (int k = 0; k < 8; ++k) {
                    c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
                }
                t[i] = c;
            }
            init = true;
        }
        return t;
    }
};

} 
