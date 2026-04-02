#pragma once

#include <cstddef>
#include <cstdint>

namespace helix {

/**
 * Platform-provided flash backend for OTA firmware receive.
 *
 * Implementations write incoming image data to the secondary MCUboot slot
 * and mark it ready for upgrade. The secondary slot base address and size
 * are encapsulated entirely in the implementation.
 */
class OtaFlashBackend {
public:
    virtual ~OtaFlashBackend() = default;

    /** Erase the entire secondary slot before a new image transfer begins. */
    virtual bool eraseSlot() = 0;

    /**
     * Write up to len bytes at byte offset within the secondary slot.
     * offset must equal the next unwritten position (sequential writes only).
     * The implementation is responsible for page alignment if required.
     */
    virtual bool writeChunk(uint32_t offset, const uint8_t* data, size_t len) = 0;

    /**
     * Signal that the image in the secondary slot is complete and should be
     * applied on the next reset.  For MCUboot overwrite-only mode this is a
     * no-op (MCUboot upgrades when it finds a valid newer image); for
     * swap-with-revert mode this writes the BOOT_MAGIC trailer.
     */
    virtual bool setPendingUpgrade() = 0;

    /** Maximum accepted image payload size in bytes. */
    virtual uint32_t slotSize() const = 0;
};

} // namespace helix
