#pragma once

#include "OtaFlashBackend.hpp"
#include <cstddef>
#include <cstdint>

namespace helix {

enum class OtaStatus : uint8_t {
    OK = 0,
    ERROR_INVALID_STATE,    ///< Operation not allowed in the current state
    ERROR_IMAGE_TOO_LARGE,  ///< imageSize exceeds the secondary slot capacity
    ERROR_WRITE_FAILED,     ///< Backend writeChunk() or eraseSlot() returned false
    ERROR_BAD_OFFSET,       ///< writeChunk offset != nextExpectedOffset (out-of-order)
    ERROR_INTEGRITY_FAILED, ///< CRC32 mismatch on commit
    ERROR_INCOMPLETE,       ///< commit() called before all declared bytes were written
    ERROR_UPGRADE_FAILED,   ///< setPendingUpgrade() returned false
};

enum class OtaState : uint8_t {
    IDLE,       ///< Ready to accept a new transfer
    RECEIVING,  ///< Transfer in progress
    COMMITTED,  ///< Image accepted and pending MCUboot upgrade on next reset
};

/**
 * OtaManager — receives a firmware image in sequential chunks over BLE,
 * writes it to the secondary MCUboot slot, verifies its integrity via CRC32,
 * and marks it ready for upgrade.
 *
 * State machine:
 *   IDLE  --begin()--> RECEIVING  --commit()--> COMMITTED
 *   Any state --abort()--> IDLE
 *   COMMITTED --begin()--> RECEIVING  (start a new transfer after committing)
 *
 * Chunks must arrive in order starting at offset 0 with no gaps.
 */
class OtaManager {
public:
    explicit OtaManager(OtaFlashBackend& backend);

    /**
     * Begin a new image transfer.
     * @param imageSize     Total image size in bytes (must fit in the secondary slot).
     * @param expectedCrc32 CRC32 the caller expects the image to have (verified on commit).
     * @return OK on success; ERROR_IMAGE_TOO_LARGE if imageSize > slotSize;
     *         ERROR_WRITE_FAILED if eraseSlot fails.
     */
    OtaStatus begin(uint32_t imageSize, uint32_t expectedCrc32);

    /**
     * Write the next sequential chunk.
     * @param offset Byte offset within the image; must equal the internal nextExpectedOffset.
     * @param data   Pointer to chunk payload.
     * @param len    Number of bytes in this chunk.
     */
    OtaStatus writeChunk(uint32_t offset, const uint8_t* data, size_t len);

    /**
     * Finalise the transfer.
     * Verifies CRC32 and that all declared bytes were received, then calls
     * setPendingUpgrade() on the backend.  On failure the manager returns to IDLE.
     */
    OtaStatus commit();

    /** Abort the current transfer and return to IDLE. */
    void abort();

    OtaState state() const { return state_; }

    /** Bytes successfully written so far (resets on begin). */
    uint32_t bytesReceived() const { return nextExpectedOffset_; }

private:
    static uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t len);

    OtaFlashBackend& backend_;
    OtaState         state_             = OtaState::IDLE;
    uint32_t         imageSize_         = 0;
    uint32_t         expectedCrc_       = 0;
    uint32_t         nextExpectedOffset_= 0;
    uint32_t         crcAccum_          = 0xFFFFFFFF; ///< Running CRC32 (pre-seeded)
};

} // namespace helix
