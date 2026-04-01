#include "NrfOtaFlashBackend.hpp"
#include "FlashWordPacker.hpp"
#include "nrf_nvmc.h"
#include <cstring>

namespace helix {

uint32_t NrfOtaFlashBackend::slotSize() const {
    return slotSize_;
}

bool NrfOtaFlashBackend::eraseSlot() {
    const uint32_t pages = slotSize_ / kPageSize;
    for (uint32_t i = 0; i < pages; ++i) {
        nrf_nvmc_page_erase(slotBase_ + i * kPageSize);
        while (!nrf_nvmc_write_done_check()) {}
    }
    return true;
}

bool NrfOtaFlashBackend::writeChunk(uint32_t offset, const uint8_t* data, size_t len) {
    if (offset + len > slotSize_) {
        return false;
    }
    return writeAligned(slotBase_ + offset, data, len);
}

bool NrfOtaFlashBackend::setPendingUpgrade() {
    // MCUboot OVERWRITE_ONLY mode: MCUboot upgrades automatically when it finds
    // a valid image with a higher version number in the secondary slot.
    // No trailer magic write is needed or desired — writing it would corrupt
    // a full-slot image (the magic would overwrite the last 16 bytes).
    return true;
}

/* static */
bool NrfOtaFlashBackend::writeAligned(uint32_t addr, const uint8_t* data, size_t len) {
    size_t written = 0;

    // Handle leading unaligned bytes by constructing the first partial word.
    const uint32_t leadBytes = addr & 3u;
    if (leadBytes != 0 && len > 0) {
        const uint32_t wordAddr = addr & ~3u;
        uint8_t existing[4];
        std::memcpy(existing, reinterpret_cast<const void*>(wordAddr), sizeof(existing));
        const size_t toCopy = 4u - leadBytes;
        const size_t n = (toCopy < len) ? toCopy : len;
        const uint32_t w = packFlashWord(existing, leadBytes, data, n);
        nrf_nvmc_word_write(wordAddr, w);
        while (!nrf_nvmc_write_done_check()) {}
        written += n;
    }

    // Write aligned 4-byte words.
    while (written + 4u <= len) {
        uint32_t w;
        memcpy(&w, data + written, 4);
        nrf_nvmc_word_write(addr + written, w);
        while (!nrf_nvmc_write_done_check()) {}
        written += 4u;
    }

    // Handle trailing partial word.
    if (written < len) {
        uint8_t existing[4];
        std::memcpy(existing, reinterpret_cast<const void*>(addr + written), sizeof(existing));
        const size_t tail = len - written;
        const uint32_t w = packFlashWord(existing, 0u, data + written, tail);
        nrf_nvmc_word_write(addr + written, w);
        while (!nrf_nvmc_write_done_check()) {}
    }

    return true;
}

} // namespace helix
