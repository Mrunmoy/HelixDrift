#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialise all ESP32-WROOM-32 board peripherals:
 *   - NVS flash
 *   - NimBLE + BLE OTA GATT service
 *   - Sensor fusion task (MPU-6050 + BMM350 → Mahony AHRS)
 *
 * Called once from app_main() before entering the idle loop.
 */
void board_wroom32_init(void);

/**
 * Mark the running OTA image as valid, cancelling the rollback timer.
 * Call after board_wroom32_init() succeeds to prevent rollback on reset.
 *
 * Weak symbol — override in tests or custom variants with a strong definition.
 */
void wroom32_ota_confirm_image(void);

#ifdef __cplusplus
}
#endif
