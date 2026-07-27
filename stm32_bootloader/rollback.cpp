// rollback.cpp
#include "rollback.h"
#include "ota_protocol.h" // reuse the shared Crc32 helper

namespace boot {

namespace {

uint32_t compute_flags_crc(const BootFlags& f) {
    // CRC over every field except crc32 itself.
    return ota::Crc32::compute(reinterpret_cast<const uint8_t*>(&f),
                                offsetof(BootFlags, crc32));
}

} // namespace

BootFlags read_flags() {
    const auto* stored = reinterpret_cast<const BootFlags*>(FLAGS_SECTOR_START);

    BootFlags flags = *stored;
    bool valid = (flags.magic == FLAGS_MAGIC) &&
                 (flags.crc32 == compute_flags_crc(flags));

    if (!valid) {
        // First boot ever, or the sector was blank/corrupted: assume bank A
        // holds the factory image and there is no pending update.
        flags = BootFlags{};
        flags.magic = FLAGS_MAGIC;
        flags.active_bank = Bank::A;
        flags.pending_bank = Bank::NONE;
        flags.boot_attempts = 0;
        flags.confirmed = 1;
        flags.crc32 = compute_flags_crc(flags);
    }
    return flags;
}

bool write_flags(BootFlags flags) {
    flags.crc32 = compute_flags_crc(flags);

    if (!erase_flags_sector()) return false;
    return flash_write(FLAGS_SECTOR_START,
                        reinterpret_cast<const uint8_t*>(&flags),
                        sizeof(flags));
}

bool mark_update_pending(Bank new_bank) {
    BootFlags flags = read_flags();
    flags.pending_bank  = new_bank;
    flags.boot_attempts = 0;
    flags.confirmed     = 0;
    return write_flags(flags);
}

BootFlags record_boot_attempt(BootFlags flags) {
    if (flags.pending_bank != Bank::NONE) {
        flags.boot_attempts++;
        write_flags(flags);
    }
    return flags;
}

bool confirm_current_firmware() {
    BootFlags flags = read_flags();
    if (flags.pending_bank == Bank::NONE) return true; // nothing to confirm

    // "swap on success": the pending bank becomes the active bank.
    flags.active_bank   = flags.pending_bank;
    flags.pending_bank   = Bank::NONE;
    flags.boot_attempts  = 0;
    flags.confirmed       = 1;
    return write_flags(flags);
}

bool should_rollback(const BootFlags& flags) {
    return flags.pending_bank != Bank::NONE &&
           !flags.confirmed &&
           flags.boot_attempts >= MAX_BOOT_ATTEMPTS;
}

} // namespace boot
