#pragma once

#include "OtaFlashBackend.hpp"

namespace helix {

/**
 * NrfOtaFlashBackend
 *
 * Implements OtaFlashBackend using the nRF52840's NVMC (non-volatile memory
 * controller) via nrfx_nvmc.
 *
 * Secondary slot layout (from xiao_nrf52840_app.ld):
 *   Base:  0x00070000  (448 KB offset from flash start)
 *   Size:  384 KB
 *
 * Flash write rules for nRF52840:
 *   - Must erase a full 4 KB page before writing (NVMC_PAGE_SIZE = 4096).
 *   - Writes must be 4-byte aligned; the driver zero-pads the final partial word.
 *   - The NVMC is single-threaded; poll nrfx_nvmc_write_done_check() after each word.
 */
class NrfOtaFlashBackend : public OtaFlashBackend {
public:
    bool     eraseSlot()                                              override;
    bool     writeChunk(uint32_t offset, const uint8_t* data, size_t len) override;
    bool     setPendingUpgrade()                                      override;
    uint32_t slotSize() const                                         override;

    static constexpr uint32_t kSecondarySlotBase = 0x00070000u;
    static constexpr uint32_t kSecondarySlotSize = 384u * 1024u;
    static constexpr uint32_t kPageSize          = 4096u;

private:
    static bool writeAligned(uint32_t addr, const uint8_t* data, size_t len);
};

} // namespace helix
