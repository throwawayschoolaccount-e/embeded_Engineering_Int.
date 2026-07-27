// flash_driver.cpp
#include "flash_driver.h"
#include "regmap_f103.h"

namespace boot {

namespace {

void flash_unlock() {
    if (FLASH_R->CR & FLASH_CR_LOCK) {
        FLASH_R->KEYR = FLASH_KEY1;
        FLASH_R->KEYR = FLASH_KEY2;
    }
}

void flash_lock() {
    FLASH_R->CR |= FLASH_CR_LOCK;
}

void wait_busy() {
    while (FLASH_R->SR & FLASH_SR_BSY) {}
}

bool erase_page(uint32_t addr) {
    wait_busy();
    FLASH_R->CR |= FLASH_CR_PER;
    FLASH_R->AR = addr;
    FLASH_R->CR |= FLASH_CR_STRT;
    wait_busy();
    FLASH_R->CR &= ~FLASH_CR_PER;
    return true;
}

} 
bool erase_bank(Bank bank) {
    flash_unlock();
    uint32_t start = bank_start(bank);
    uint32_t pages = (bank_size(bank) + PAGE_SIZE - 1) / PAGE_SIZE;

    bool ok = true;
    for (uint32_t p = 0; p < pages; ++p) {
        if (!erase_page(start + p * PAGE_SIZE)) { ok = false; break; }
    }
    flash_lock();
    return ok;
}

bool flash_write(uint32_t address, const uint8_t* data, size_t len) {
    flash_unlock();

    size_t i = 0;
    while (i < len) {
        uint16_t half_word;
        if (i + 1 < len) {
            half_word = static_cast<uint16_t>(data[i] | (data[i + 1] << 8));
        } else {
            half_word = static_cast<uint16_t>(data[i] | (0xFFu << 8)); // pad odd trailing byte
        }

        wait_busy();
        FLASH_R->CR |= FLASH_CR_PG;
        *reinterpret_cast<volatile uint16_t*>(address + i) = half_word;
        wait_busy();
        FLASH_R->CR &= ~FLASH_CR_PG;

        i += 2;
    }

    flash_lock();
    return true;
}

bool erase_flags_sector() {
    flash_unlock();
    uint32_t pages = (FLAGS_SECTOR_SIZE + PAGE_SIZE - 1) / PAGE_SIZE;

    bool ok = true;
    for (uint32_t p = 0; p < pages; ++p) {
        if (!erase_page(FLAGS_SECTOR_START + p * PAGE_SIZE)) { ok = false; break; }
    }
    flash_lock();
    return ok;
}

} // namespace boot
