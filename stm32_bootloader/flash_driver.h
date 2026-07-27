// flash_driver.h
// Flash memory map and erase/program primitives for the dual-bank bootloader.
//
// Sized for an STM32F103xx "Medium Density" part (e.g. NUCLEO-F103RB):
// 128 KB total flash, erased in 1 KB pages. 128 KB is tight for a dual-bank
// OTA bootloader with Ed25519 verification built in, so the split below
// gives the bootloader itself extra room (crypto code adds up) and leaves
// two ~51 KB banks for your actual application. If your application needs
// more room, you'd need a bigger-flash part (F103RC/RE, or an F4).
#pragma once

#include <cstdint>
#include <cstddef>

namespace boot {

// ---- Memory map: STM32F103xx_MD, 128 KB flash, 1 KB pages -------------
constexpr uint32_t PAGE_SIZE = 0x400; // 1 KB, medium-density STM32F1 page size

constexpr uint32_t BOOTLOADER_START = 0x08000000;
constexpr uint32_t BOOTLOADER_SIZE  = 0x6000;  // 24 KB for bootloader + crypto lib

constexpr uint32_t BANK_A_START     = 0x08006000; // "Running FW" bank
constexpr uint32_t BANK_A_SIZE      = 0xCC00;      // 51 KB
constexpr uint32_t BANK_B_START     = 0x08012C00; // "New FW Target" bank
constexpr uint32_t BANK_B_SIZE      = 0xCC00;      // 51 KB

constexpr uint32_t FLAGS_SECTOR_START = 0x0801F800; // last 2 pages of flash
constexpr uint32_t FLAGS_SECTOR_SIZE  = 0x800;       // 2 KB (2 pages)

enum class Bank : uint8_t { NONE = 0, A = 1, B = 2 };

inline uint32_t bank_start(Bank b) {
    return b == Bank::A ? BANK_A_START : BANK_B_START;
}
inline uint32_t bank_size(Bank b) {
    return b == Bank::A ? BANK_A_SIZE : BANK_B_SIZE;
}
inline Bank other_bank(Bank b) {
    return b == Bank::A ? Bank::B : Bank::A;
}

// Erases every page covered by `bank`. Must be called before writing a
// fresh image into that bank.
bool erase_bank(Bank bank);

// Programs `len` bytes starting at `address` (32-bit word granularity).
bool flash_write(uint32_t address, const uint8_t* data, size_t len);

// Erases the 2-page flags sector.
bool erase_flags_sector();

} 
