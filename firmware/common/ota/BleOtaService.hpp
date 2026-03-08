#pragma once

#include "IOtaManager.hpp"
#include <cstddef>
#include <cstdint>

namespace helix {

/**
 * BleOtaService — platform-independent handler for a 3-characteristic BLE OTA GATT service.
 *
 * Characteristic layout:
 *
 *   OTA_CTRL (write):
 *     [0x01][size:4 LE][crc32:4 LE]   → begin(size, crc)
 *     [0x02]                           → abort()
 *     [0x03]                           → commit()
 *
 *   OTA_DATA (write-no-rsp):
 *     [offset:4 LE][payload...]        → writeChunk(offset, payload, len)
 *
 *   OTA_STATUS (read / notify):
 *     [state:1][bytes_received:4 LE][last_status:1]   → 6 bytes total
 *
 * This class has no BLE stack dependency. Platform-specific code
 * (NimBLE attribute callbacks for ESP32, SoftDevice for nRF) calls
 * handleControlWrite(), handleDataWrite(), and getStatus().
 */
class BleOtaService {
public:
    static constexpr uint8_t CMD_BEGIN  = 0x01u;
    static constexpr uint8_t CMD_ABORT  = 0x02u;
    static constexpr uint8_t CMD_COMMIT = 0x03u;

    static constexpr size_t kStatusLen       = 6u;
    static constexpr size_t kCtrlBeginMinLen = 9u;
    static constexpr size_t kDataHeaderLen   = 4u;

    explicit BleOtaService(IOtaManager& manager) : m_mgr(manager) {}

    /**
     * Handle a write to the OTA_CTRL characteristic.
     * @return OtaStatus::OK on success; error code otherwise.
     */
    OtaStatus handleControlWrite(const uint8_t* data, size_t len);

    /**
     * Handle a write to the OTA_DATA characteristic.
     * @return OtaStatus::OK on success; error code otherwise.
     */
    OtaStatus handleDataWrite(const uint8_t* data, size_t len);

    /**
     * Fill @p out with the current status (6 bytes).
     * Sets *outLen to the number of bytes written.
     * Safe to call with out==nullptr (no-op).
     */
    void getStatus(uint8_t* out, size_t* outLen) const;

private:
    static uint32_t readU32Le(const uint8_t* p);

    IOtaManager& m_mgr;
    OtaStatus    m_lastStatus{OtaStatus::OK};
};

} // namespace helix
