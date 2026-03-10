#pragma once

#include "EspOtaOpsInterface.hpp"
#include "OtaFlashBackend.hpp"

namespace helix
{

/**
 * OtaFlashBackend implementation for ESP32 using ESP-IDF esp_ota_ops API.
 *
 * See firmware/common/ota/OtaFlashBackend.hpp for the contract.
 * Identical implementation to the ESP32-S3 variant; shared via copy to keep
 * each example self-contained. If the logic needs to change, update both and
 * consider promoting to firmware/common/.
 */
class Esp32OtaFlashBackend final : public OtaFlashBackend
{
public:
    explicit Esp32OtaFlashBackend(EspOtaOpsInterface& ops,
                                  uint32_t            slotSizeBytes)
        : m_ops(ops)
        , m_slotSizeBytes(slotSizeBytes)
    {}

    bool     eraseSlot()                                                   override;
    bool     writeChunk(uint32_t offset, const uint8_t* data, size_t len) override;
    bool     setPendingUpgrade()                                           override;
    uint32_t slotSize() const                                              override;

private:
    EspOtaOpsInterface&    m_ops;
    uint32_t               m_slotSizeBytes;
    esp_ota_handle_t       m_handle{0};
    const esp_partition_t* m_partition{nullptr};
    bool                   m_active{false};

    void closeSession();
};

} // namespace helix
