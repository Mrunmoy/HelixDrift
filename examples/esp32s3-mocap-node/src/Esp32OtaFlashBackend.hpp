#pragma once

#include "EspOtaOpsInterface.hpp"
#include "OtaFlashBackend.hpp"

namespace helix {

/**
 * OtaFlashBackend implementation for ESP32-S3 using ESP-IDF's esp_ota_ops API.
 *
 * Mapping to OtaFlashBackend contract:
 *   eraseSlot()              → getNextUpdatePartition() + esp_ota_begin()
 *   writeChunk(off, data, n) → esp_ota_write()  (offset is sequential in ESP-IDF)
 *   setPendingUpgrade()      → esp_ota_end()    + esp_ota_set_boot_partition()
 *   slotSize()               → returns the compile-time configured slot size
 *
 * The OTA slot size is fixed at construction time (from partitions_ota.csv) so that
 * slotSize() is always available before eraseSlot() is called.
 *
 * Calling eraseSlot() while a session is already active closes the old session
 * first to avoid an esp_ota_handle_t leak.
 *
 * Thread-safety: not re-entrant. OtaManager serialises all calls.
 */
class Esp32OtaFlashBackend final : public OtaFlashBackend {
public:
    explicit Esp32OtaFlashBackend(EspOtaOpsInterface& ops,
                                  uint32_t            slotSizeBytes)
        : m_ops(ops), m_slotSizeBytes(slotSizeBytes) {}

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
