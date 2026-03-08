#include "OtaManager.hpp"

namespace helix {

OtaManager::OtaManager(OtaFlashBackend& backend)
    : backend_(backend) {}

OtaStatus OtaManager::begin(uint32_t imageSize, uint32_t expectedCrc32) {
    if (state_ == OtaState::RECEIVING) {
        return OtaStatus::ERROR_INVALID_STATE;
    }
    if (imageSize == 0 || imageSize > backend_.slotSize()) {
        return OtaStatus::ERROR_IMAGE_TOO_LARGE;
    }
    if (!backend_.eraseSlot()) {
        return OtaStatus::ERROR_WRITE_FAILED;
    }
    imageSize_          = imageSize;
    expectedCrc_        = expectedCrc32;
    nextExpectedOffset_ = 0;
    crcAccum_           = 0xFFFFFFFF;
    state_              = OtaState::RECEIVING;
    return OtaStatus::OK;
}

OtaStatus OtaManager::writeChunk(uint32_t offset, const uint8_t* data, size_t len) {
    if (state_ != OtaState::RECEIVING) {
        return OtaStatus::ERROR_INVALID_STATE;
    }
    if (offset != nextExpectedOffset_) {
        return OtaStatus::ERROR_BAD_OFFSET;
    }
    if (len == 0) {
        return OtaStatus::OK;
    }
    if (offset + static_cast<uint32_t>(len) > imageSize_) {
        return OtaStatus::ERROR_BAD_OFFSET;
    }
    if (!backend_.writeChunk(offset, data, len)) {
        state_              = OtaState::IDLE;
        nextExpectedOffset_ = 0;
        return OtaStatus::ERROR_WRITE_FAILED;
    }
    crcAccum_           = crc32Update(crcAccum_, data, len);
    nextExpectedOffset_ += static_cast<uint32_t>(len);
    return OtaStatus::OK;
}

OtaStatus OtaManager::commit() {
    if (state_ != OtaState::RECEIVING) {
        return OtaStatus::ERROR_INVALID_STATE;
    }
    if (nextExpectedOffset_ != imageSize_) {
        state_ = OtaState::IDLE;
        return OtaStatus::ERROR_INCOMPLETE;
    }
    const uint32_t finalCrc = crcAccum_ ^ 0xFFFFFFFF;
    if (finalCrc != expectedCrc_) {
        state_ = OtaState::IDLE;
        return OtaStatus::ERROR_INTEGRITY_FAILED;
    }
    if (!backend_.setPendingUpgrade()) {
        state_ = OtaState::IDLE;
        return OtaStatus::ERROR_UPGRADE_FAILED;
    }
    state_ = OtaState::COMMITTED;
    return OtaStatus::OK;
}

void OtaManager::abort() {
    state_              = OtaState::IDLE;
    nextExpectedOffset_ = 0;
    crcAccum_           = 0xFFFFFFFF;
}

/* static */ uint32_t OtaManager::crc32Update(uint32_t crc, const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            crc = (crc & 1u) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
        }
    }
    return crc;
}

} // namespace helix
