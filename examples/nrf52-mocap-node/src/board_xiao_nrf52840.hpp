#pragma once

#include "NrfSdk.hpp"
#include <cstdint>

extern const nrfx_twim_t g_twim0;
extern const nrfx_twim_t g_twim1;

bool xiao_board_init_i2c();

// ---------------------------------------------------------------------------
// OTA board hooks (weak defaults provided in board_xiao_nrf52840.cpp).
// Implement these in your BLE stack layer to enable firmware updates.
// ---------------------------------------------------------------------------

/**
 * Called by the OTA layer to signal that the device has booted successfully
 * into a new image and MCUboot should mark it as confirmed.
 * On swap-with-revert builds this writes image_ok=1 to the primary slot trailer.
 * On OVERWRITE_ONLY builds this is a no-op but should still be called.
 */
extern "C" void xiao_ota_confirm_image();

/**
 * Deliver an OTA control command byte to the OTA manager.
 * Commands:
 *   0 = none (polling, returns 0)
 *   1 = begin    – payload: 8 bytes (4-byte image size LE + 4-byte CRC32 LE)
 *   2 = commit
 *   3 = abort
 * Returns the OtaStatus byte from the last operation (0 = OK).
 */
extern "C" uint8_t xiao_ota_control(uint8_t cmd, const uint8_t* payload, size_t payloadLen);

/**
 * Deliver a raw OTA data chunk to the OTA manager.
 * @param offset Byte offset within the image (sequential, no gaps).
 * @param data   Pointer to chunk payload.
 * @param len    Number of bytes in this chunk.
 * Returns 0 on success, non-zero on error.
 */
extern "C" uint8_t xiao_ota_write_chunk(uint32_t offset, const uint8_t* data, size_t len);

// ---------------------------------------------------------------------------
// Existing BLE / sensor hooks (unchanged)
// ---------------------------------------------------------------------------
extern "C" bool sf_mocap_ble_notify(const uint8_t* data, size_t len);
extern "C" uint8_t sf_mocap_calibration_command();
extern "C" bool sf_mocap_sync_anchor(uint64_t* localUs, uint64_t* remoteUs);
extern "C" bool sf_mocap_health_sample(
    uint16_t* batteryMv,
    uint8_t*  batteryPercent,
    uint8_t*  linkQuality,
    uint16_t* droppedFrames,
    uint8_t*  flags);

