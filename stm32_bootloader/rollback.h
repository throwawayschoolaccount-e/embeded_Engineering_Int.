// rollback.h
// Persistent boot state, stored in a dedicated flash sector so it survives
// power loss at any point. This implements the "Rollback Logic (Safe on
// power loss)" and "Boot Decision (Check update flag)" boxes from the
// diagram.
#pragma once

#include <cstdint>
#include "flash_driver.h"

namespace boot {

constexpr uint32_t FLAGS_MAGIC = 0x4F544131; // "OTA1"
constexpr uint8_t  MAX_BOOT_ATTEMPTS = 3;

#pragma pack(push, 1)
struct BootFlags {
    uint32_t magic;          // FLAGS_MAGIC if this sector holds valid data
    Bank     active_bank;    // bank containing firmware known to be good
    Bank     pending_bank;   // Bank::NONE unless a new image is awaiting confirmation
    uint8_t  boot_attempts;  // attempts made to boot `pending_bank` so far
    uint8_t  confirmed;      // 1 once the running application calls confirm_boot()
    uint32_t crc32;          // CRC32 over the fields above (excluding this one)
};
#pragma pack(pop)

// Reads flags from flash. If the sector is blank/corrupt (e.g. very first
// boot), returns a default: active_bank = A, pending_bank = NONE.
BootFlags read_flags();

// Erases and rewrites the whole flags sector with `flags` (with crc32 filled in).
bool write_flags(BootFlags flags);

// Called by uart_receiver after a signature-verified image lands in the
// inactive bank: marks it pending and resets the attempt counter.
bool mark_update_pending(Bank new_bank);

// Called at the top of main() before deciding what to boot: increments the
// attempt counter for a pending image. Returns the updated flags.
BootFlags record_boot_attempt(BootFlags flags);

// Called by the *application* (not the bootloader) once it has verified
// it's healthy (self-test passed, watchdog fed, etc). This is what
// distinguishes "swap on success" from "revert to bank A" in the diagram -
// without this call, the bootloader assumes the new image is bad.
bool confirm_current_firmware();

// True if `flags.pending_bank` has exceeded MAX_BOOT_ATTEMPTS without being
// confirmed - the bootloader should fall back to `active_bank`.
bool should_rollback(const BootFlags& flags);

} 
