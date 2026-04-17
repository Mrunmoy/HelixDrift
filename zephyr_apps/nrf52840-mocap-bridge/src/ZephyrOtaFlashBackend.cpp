#include "ZephyrOtaFlashBackend.hpp"

#include <string.h>
#include <zephyr/autoconf.h>
#include <zephyr/devicetree/fixed-partitions.h>
#include <zephyr/storage/flash_map.h>

namespace helix {

namespace {
constexpr uint32_t kFooterSize = CONFIG_MCUBOOT_UPDATE_FOOTER_SIZE;
constexpr uint32_t kBootMaxAlign = 8u;
constexpr uint32_t kSwapInfoOffsetFromEnd = 5u * kBootMaxAlign;
constexpr uint32_t kImageOkOffsetFromEnd = 3u * kBootMaxAlign;
constexpr uint32_t kMagicOffsetFromEnd = 16u;
constexpr uint8_t kMagic[16] = {
    0x77u, 0xc2u, 0x95u, 0xf3u,
    0x60u, 0xd2u, 0xefu, 0x7fu,
    0x35u, 0x52u, 0x50u, 0x0fu,
    0x2cu, 0xb6u, 0x79u, 0x80u
};
constexpr uint8_t kSwapTypeTest = 0x02u;

static_assert(kFooterSize == 0x30u, "Unexpected MCUboot update footer size");
}

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

    uint8_t swapInfo[kBootMaxAlign];
    memset(swapInfo, 0xFF, sizeof(swapInfo));
    swapInfo[0] = kSwapTypeTest;

    uint8_t magic[sizeof(kMagic)];
    memcpy(magic, kMagic, sizeof(magic));

    const off_t swapInfoOffset = static_cast<off_t>(slot1_->fa_size - kSwapInfoOffsetFromEnd);
    const off_t imageOkOffset = static_cast<off_t>(slot1_->fa_size - kImageOkOffsetFromEnd);
    const off_t magicOffset = static_cast<off_t>(slot1_->fa_size - kMagicOffsetFromEnd);

    uint8_t imageOkProbe = 0xFFu;
    if (flash_area_read(slot1_, imageOkOffset, &imageOkProbe, sizeof(imageOkProbe)) != 0) {
        return false;
    }

    return flash_area_write(slot1_, swapInfoOffset, swapInfo, sizeof(swapInfo)) == 0
        && flash_area_write(slot1_, magicOffset, magic, sizeof(magic)) == 0;
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
