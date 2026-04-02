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
    const ::flash_area* slot1_ = nullptr;
};

} // namespace helix
