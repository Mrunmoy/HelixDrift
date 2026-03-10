#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "board_wroom32.h"

static const char* TAG = "helix_wroom32";

void app_main(void)
{
    ESP_LOGI(TAG, "HelixDrift ESP32-WROOM-32 mocap node");

    board_wroom32_init();

    /*
     * Confirm the running image is valid. This cancels the OTA rollback timer
     * so the bootloader does not revert on the next reset.
     */
    wroom32_ota_confirm_image();

    while (1)
    {
        ESP_LOGI(TAG, "alive");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
