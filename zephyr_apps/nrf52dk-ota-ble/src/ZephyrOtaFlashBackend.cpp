#include "ZephyrOtaFlashBackend.hpp"

#include <zephyr/dfu/mcuboot.h>
#include <zephyr/devicetree/fixed-partitions.h>
#include <zephyr/storage/flash_map.h>

namespace helix {

bool ZephyrOtaFlashBackend::init() {
    if (slot1_) {
        return true;
    }
    return flash_area_open(FIXED_PARTITION_ID(slot1_partition), &slot1_) == 0;
}

uint32_t ZephyrOtaFlashBackend::slotSize() const {
    return slot1_ ? slot1_->fa_size : 0u;
}

bool ZephyrOtaFlashBackend::eraseSlot() {
    return slot1_ && flash_area_erase(slot1_, 0, slot1_->fa_size) == 0;
}

bool ZephyrOtaFlashBackend::writeChunk(uint32_t offset, const uint8_t* data, size_t len) {
    return slot1_
        && offset + len <= slot1_->fa_size
        && flash_area_write(slot1_, offset, data, len) == 0;
}

bool ZephyrOtaFlashBackend::setPendingUpgrade() {
    return boot_request_upgrade(BOOT_UPGRADE_PERMANENT) == 0;
}

} // namespace helix
