#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "board_xiao_esp32s3.h"

static const char* TAG = "helix_esp32s3";

void app_main(void) {
    ESP_LOGI(TAG, "HelixDrift ESP32-S3 mocap node bootstrap");

    board_xiao_esp32s3_init();

    /*
     * Confirm the running image is valid after peripherals initialise
     * successfully. This cancels the OTA rollback timer so that the
     * bootloader does not revert to the previous image on the next reset.
     */
    xiao_ota_confirm_image();

    while (1) {
        ESP_LOGI(TAG, "System alive");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
