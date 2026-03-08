#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialise the XIAO ESP32-S3 board peripherals (NimBLE, GATT OTA service).
 * Called from app_main() before entering the main loop.
 */
void board_xiao_esp32s3_init(void);

/**
 * Called once after all sensors/peripherals have initialised successfully.
 * Marks the running OTA image as valid (cancels the rollback timer).
 * Applications should call this only after confirming the firmware is healthy.
 *
 * This is a weak symbol; the default implementation calls
 * esp_ota_mark_app_valid_cancel_rollback().  Override in tests or custom board
 * variants by providing a strong definition.
 */
void xiao_ota_confirm_image(void);

#ifdef __cplusplus
}
#endif
