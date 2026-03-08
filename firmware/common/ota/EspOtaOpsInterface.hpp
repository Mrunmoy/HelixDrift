#pragma once

#include <cstdint>
#include <cstddef>

#include "esp_ota_ops.h"

namespace helix {

/**
 * Pure interface wrapping the ESP-IDF esp_ota_ops C API.
 *
 * Allows Esp32OtaFlashBackend to be unit-tested on the host via a mock
 * without any dependency on real ESP-IDF hardware drivers.
 *
 * Production implementation: EspOtaOpsReal (calls actual esp_ota_ops.h).
 * Test implementation: MockEspOtaOps (GMock).
 */
class EspOtaOpsInterface {
public:
    virtual ~EspOtaOpsInterface() = default;

    /**
     * Returns the next OTA partition to write to (cycles OTA_0 → OTA_1 → …).
     * @param start  If nullptr, uses the currently running partition as hint.
     * @return Pointer to the target partition, or nullptr on failure.
     */
    virtual const esp_partition_t* getNextUpdatePartition(
        const esp_partition_t* start) = 0;

    /**
     * Begins an OTA write session for the given partition.
     * @param partition   Target OTA partition.
     * @param image_size  Total image size in bytes (OTA_SIZE_UNKNOWN = 0xFFFFFFFF).
     * @param out_handle  Returned session handle.
     * @return ESP_OK on success.
     */
    virtual esp_err_t begin(const esp_partition_t* partition,
                            size_t                 image_size,
                            esp_ota_handle_t*      out_handle) = 0;

    /**
     * Appends data to the current OTA session. Must be called sequentially.
     * @return ESP_OK on success.
     */
    virtual esp_err_t write(esp_ota_handle_t handle,
                            const void*      data,
                            size_t           size) = 0;

    /**
     * Finalises the OTA session. Validates the written image.
     * @return ESP_OK if the image is valid and the session is closed cleanly.
     */
    virtual esp_err_t end(esp_ota_handle_t handle) = 0;

    /**
     * Sets the given partition as the boot target on next reset.
     * @return ESP_OK on success.
     */
    virtual esp_err_t setBootPartition(const esp_partition_t* partition) = 0;
};

} // namespace helix
