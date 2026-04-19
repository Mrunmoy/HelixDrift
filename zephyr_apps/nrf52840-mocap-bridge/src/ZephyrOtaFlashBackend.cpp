#include "ZephyrOtaFlashBackend.hpp"

#include <string.h>
#include <zephyr/autoconf.h>
#include <zephyr/devicetree/fixed-partitions.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/storage/flash_map.h>

#if __has_include(<pm_config.h>)
#include <pm_config.h>
#endif

namespace helix {

bool ZephyrOtaFlashBackend::init() {
    if (slot1_) {
        return true;
    }
    /* Use the partition manager's mcuboot_secondary ID (from pm_static.yml)
     * rather than DTS slot1_partition. The DTS label and PM partition can
     * resolve to DIFFERENT flash addresses; MCUboot & imgtool use the PM
     * definition, so we must too.  On this nRF52840 build:
     *   DTS slot1_partition = 0x82000 (wrong)
     *   PM_MCUBOOT_SECONDARY = 0x85000 (right) */
#if defined(PM_MCUBOOT_SECONDARY_ID)
    return flash_area_open(PM_MCUBOOT_SECONDARY_ID, &slot1_) == 0;
#else
    return flash_area_open(FIXED_PARTITION_ID(slot1_partition), &slot1_) == 0;
#endif
}

uint32_t ZephyrOtaFlashBackend::slotSize() const {
    return slot1_ ? slot1_->fa_size : 0u;
}

bool ZephyrOtaFlashBackend::eraseSlot() {
    if (!slot1_ || flash_area_erase(slot1_, 0, slot1_->fa_size) != 0) {
        return false;
    }
    nextWriteOffset_ = 0u;
    bufferBaseOffset_ = 0u;
    bufferedBytes_ = 0u;
    return true;
}

bool ZephyrOtaFlashBackend::writeChunk(uint32_t offset, const uint8_t* data, size_t len) {
    if (!slot1_ || !data || offset != nextWriteOffset_ || offset + len > slot1_->fa_size) {
        return false;
    }

    size_t consumed = 0u;
    while (consumed < len) {
        if (bufferedBytes_ == 0u) {
            bufferBaseOffset_ = offset + static_cast<uint32_t>(consumed);
        }

        const size_t space = kFlashBatchBytes - bufferedBytes_;
        const size_t copyLen = (len - consumed < space) ? (len - consumed) : space;
        memcpy(buffer_ + bufferedBytes_, data + consumed, copyLen);
        bufferedBytes_ += copyLen;
        consumed += copyLen;

        if (bufferedBytes_ == kFlashBatchBytes && !flushBuffered()) {
            return false;
        }
    }

    nextWriteOffset_ += static_cast<uint32_t>(len);
    return true;
}

bool ZephyrOtaFlashBackend::setPendingUpgrade() {
    if (!flushBuffered() || !slot1_) {
        return false;
    }
    /* Use Zephyr's official API to request the upgrade. BOOT_UPGRADE_PERMANENT
     * (nonzero) so MCUboot doesn't require a confirm after reboot. */
    return boot_request_upgrade(BOOT_UPGRADE_PERMANENT) == 0;
}

bool ZephyrOtaFlashBackend::flushBuffered() {
    if (!slot1_ || bufferedBytes_ == 0u) {
        return true;
    }

    const uint32_t align = flash_area_align(slot1_);
    size_t writeLen = bufferedBytes_;
    if (align > 1u) {
        const size_t remainder = writeLen % align;
        if (remainder != 0u) {
            const size_t paddedLen = writeLen + (align - remainder);
            if (paddedLen > sizeof(buffer_)) {
                return false;
            }
            memset(buffer_ + writeLen, 0xFF, paddedLen - writeLen);
            writeLen = paddedLen;
        }
    }

    if (flash_area_write(slot1_, bufferBaseOffset_, buffer_, writeLen) != 0) {
        return false;
    }

    bufferedBytes_ = 0u;
    return true;
}

} // namespace helix
