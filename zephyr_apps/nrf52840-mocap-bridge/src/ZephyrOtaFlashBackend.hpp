#pragma once

#include "OtaFlashBackend.hpp"

struct flash_area;

namespace helix {

class ZephyrOtaFlashBackend final : public OtaFlashBackend {
public:
    bool init();
    bool eraseSlot() override;
    bool writeChunk(uint32_t offset, const uint8_t* data, size_t len) override;
    bool setPendingUpgrade() override;
    uint32_t slotSize() const override;

private:
    bool flushBuffered();

    const ::flash_area* slot1_ = nullptr;
    uint32_t            nextWriteOffset_ = 0u;
    uint32_t            bufferBaseOffset_ = 0u;
    size_t              bufferedBytes_ = 0u;
    static constexpr size_t kFlashBatchBytes = 256u;
    uint8_t             buffer_[kFlashBatchBytes] = {};
};

} // namespace helix
