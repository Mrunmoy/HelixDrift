#include "NrfOtaFlashBackend.hpp"
#include "nrfx_nvmc.h"
#include <cstring>

namespace helix {

uint32_t NrfOtaFlashBackend::slotSize() const {
    return kSecondarySlotSize;
}

bool NrfOtaFlashBackend::eraseSlot() {
    const uint32_t pages = kSecondarySlotSize / kPageSize;
    for (uint32_t i = 0; i < pages; ++i) {
        nrfx_nvmc_page_erase(kSecondarySlotBase + i * kPageSize);
        // Poll until erase is done (nrfx stubs return true immediately).
        while (!nrfx_nvmc_write_done_check()) {}
    }
    return true;
}

bool NrfOtaFlashBackend::writeChunk(uint32_t offset, const uint8_t* data, size_t len) {
    if (offset + len > kSecondarySlotSize) {
        return false;
    }
    return writeAligned(kSecondarySlotBase + offset, data, len);
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
        uint8_t word[4] = {0xFF, 0xFF, 0xFF, 0xFF};
        const size_t toCopy = 4u - leadBytes;
        const size_t n = (toCopy < len) ? toCopy : len;
        for (size_t i = 0; i < n; ++i) {
            word[leadBytes + i] = data[i];
        }
        uint32_t w;
        memcpy(&w, word, 4);
        nrfx_nvmc_word_write(wordAddr, w);
        while (!nrfx_nvmc_write_done_check()) {}
        written += n;
    }

    // Write aligned 4-byte words.
    while (written + 4u <= len) {
        uint32_t w;
        memcpy(&w, data + written, 4);
        nrfx_nvmc_word_write(addr + written, w);
        while (!nrfx_nvmc_write_done_check()) {}
        written += 4u;
    }

    // Handle trailing partial word.
    if (written < len) {
        uint8_t word[4] = {0xFF, 0xFF, 0xFF, 0xFF};
        const size_t tail = len - written;
        for (size_t i = 0; i < tail; ++i) {
            word[i] = data[written + i];
        }
        uint32_t w;
        memcpy(&w, word, 4);
        nrfx_nvmc_word_write(addr + written, w);
        while (!nrfx_nvmc_write_done_check()) {}
    }

    return true;
}

} // namespace helix
