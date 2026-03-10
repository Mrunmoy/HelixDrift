#pragma once

#include "IDelayProvider.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"

namespace helix
{

/**
 * IDelayProvider implementation backed by the ESP-IDF FreeRTOS and ROM APIs.
 *
 * delayMs  → vTaskDelay (yields to scheduler, accurate to tick resolution)
 * delayUs  → esp_rom_delay_us (busy-spin, suitable for short delays in init)
 * getTimestampUs → esp_timer_get_time (monotonic, 64-bit µs counter)
 */
class EspDelayProvider final : public sf::IDelayProvider
{
public:
    void delayMs(uint32_t ms) override
    {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }

    void delayUs(uint32_t us) override
    {
        esp_rom_delay_us(us);
    }

    uint64_t getTimestampUs() override
    {
        return static_cast<uint64_t>(esp_timer_get_time());
    }
};

} // namespace helix
