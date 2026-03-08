#pragma once

/*
 * Host-build stub for esp_ota_ops.h.
 *
 * Provides only the types and constants needed to compile Esp32OtaFlashBackend
 * and EspOtaOpsInterface on a host (x86) machine. No function implementations
 * are provided here; production code routes through EspOtaOpsInterface whose
 * real implementation calls the actual ESP-IDF APIs.
 *
 * MUST NOT be included in ESP-IDF target builds — guard with:
 *   if(ESP32_STUB)
 *     target_include_directories(... tools/esp/stubs/include)
 *     target_compile_definitions(... ESP32_STUB)
 *   endif()
 *
 * If accidentally included without ESP32_STUB, a compile-time error fires.
 */

#ifndef ESP32_STUB
#  error "esp_ota_ops.h stub included in a non-stub build. " \
         "Remove tools/esp/stubs/include from your include path for hardware builds."
#endif

#include <stdint.h>
#include <stddef.h>

typedef int32_t  esp_err_t;
typedef uint32_t esp_ota_handle_t;

#define ESP_OK   ((esp_err_t)0)
#define ESP_FAIL ((esp_err_t)-1)

/* Minimal partition descriptor — matches the fields used by Esp32OtaFlashBackend. */
typedef struct {
    uint8_t  type;
    uint8_t  subtype;
    uint32_t address;
    uint32_t size;
    char     label[17];
    bool     encrypted;
} esp_partition_t;
