#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char* TAG = "helix_esp32s3";

void app_main(void) {
    ESP_LOGI(TAG, "HelixDrift ESP32-S3 mocap node bootstrap");
    while (1) {
        ESP_LOGI(TAG, "System alive");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
