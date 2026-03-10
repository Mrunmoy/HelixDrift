#include "board_wroom32.h"

#include <cstring>

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "EspI2CBus.hpp"
#include "EspDelayProvider.hpp"
#include "SensorFusionTask.hpp"
#include "Esp32OtaFlashBackend.hpp"
#include "IOtaManager.hpp"
#include "BleOtaService.hpp"

#include "MPU6050.hpp"
#include "BMM350.hpp"

static const char* TAG = "board_wroom32";

// ── OTA slot size: two × 1920 KB slots in 4 MB flash ─────────────────────
static constexpr uint32_t kOtaSlotSize = 0x1E0000u;

// ── EspOtaOps: real ESP-IDF OTA calls ────────────────────────────────────
class EspOtaOpsReal final : public helix::EspOtaOpsInterface
{
public:
    const esp_partition_t* getNextUpdatePartition(const esp_partition_t* s) override
    {
        return esp_ota_get_next_update_partition(s);
    }
    esp_err_t begin(const esp_partition_t* p, size_t sz, esp_ota_handle_t* h) override
    {
        return esp_ota_begin(p, sz, h);
    }
    esp_err_t write(esp_ota_handle_t h, const void* d, size_t n) override
    {
        return esp_ota_write(h, d, n);
    }
    esp_err_t end(esp_ota_handle_t h) override
    {
        return esp_ota_end(h);
    }
    esp_err_t setBootPartition(const esp_partition_t* p) override
    {
        return esp_ota_set_boot_partition(p);
    }
};

// ── Static object graph ───────────────────────────────────────────────────

static EspOtaOpsReal               g_otaOps;
static helix::Esp32OtaFlashBackend g_otaBackend{g_otaOps, kOtaSlotSize};
static helix::OtaManager           g_otaManager{g_otaBackend};
static helix::OtaManagerAdapter    g_otaManagerAdapter{g_otaManager};
static helix::BleOtaService        g_bleOtaService{g_otaManagerAdapter};

static helix::EspI2CBus::Config kI2CCfg = {
    .port   = I2C_NUM_0,
    .sdaPin = GPIO_NUM_32,
    .sclPin = GPIO_NUM_33,
    .clkHz  = 400'000,
};

static helix::EspI2CBus        g_i2cBus{kI2CCfg};
static helix::EspDelayProvider g_delay;

static sf::MPU6050Config kImuCfg = {
    .accelRange    = sf::AccelRange::G2,
    .gyroRange     = sf::GyroRange::DPS500,
    .dlpf          = sf::DlpfBandwidth::BW44,
    .sampleRateDiv = 4,   // 1000 / (1 + 4) = 200 Hz
    .i2cBypass     = false,
    .address       = 0x68,
};

static sf::MPU6050 g_imu{g_i2cBus, g_delay, kImuCfg};
static sf::BMM350  g_mag{g_i2cBus, g_delay};

// SensorFusionTask is heap-allocated so we can decide mag pointer at runtime.
static helix::SensorFusionTask* g_sfTask = nullptr;

// ── OTA GATT UUIDs ────────────────────────────────────────────────────────

/* Service: 12345678-1234-1234-1234-1234567890AB */
static const ble_uuid128_t kOtaSvcUuid = BLE_UUID128_INIT(
    0xAB, 0x90, 0x78, 0x56, 0x34, 0x12, 0x34, 0x12,
    0x34, 0x12, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

/* OTA_CTRL: ...90AC */
static const ble_uuid128_t kOtaCtrlUuid = BLE_UUID128_INIT(
    0xAC, 0x90, 0x78, 0x56, 0x34, 0x12, 0x34, 0x12,
    0x34, 0x12, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

/* OTA_DATA: ...90AD */
static const ble_uuid128_t kOtaDataUuid = BLE_UUID128_INIT(
    0xAD, 0x90, 0x78, 0x56, 0x34, 0x12, 0x34, 0x12,
    0x34, 0x12, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

/* OTA_STATUS: ...90AE */
static const ble_uuid128_t kOtaStatusUuid = BLE_UUID128_INIT(
    0xAE, 0x90, 0x78, 0x56, 0x34, 0x12, 0x34, 0x12,
    0x34, 0x12, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

// ── GATT callbacks ────────────────────────────────────────────────────────
// NimBLE structs use C-style designated initializers with unspecified fields
// zero-initialized by the language; suppress the GCC lint warning.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

static int ota_ctrl_write_cb(uint16_t, uint16_t,
                              struct ble_gatt_access_ctxt* ctxt, void*)
{
    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    uint8_t  buf[64];
    if (len > sizeof(buf)) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    ble_hs_mbuf_to_flat(ctxt->om, buf, len, &len);
    g_bleOtaService.handleControlWrite(buf, len);
    return 0;
}

static int ota_data_write_cb(uint16_t, uint16_t,
                              struct ble_gatt_access_ctxt* ctxt, void*)
{
    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    /* ESP32 BLE 4.2: max ATT payload 251 B with DLE. */
    uint8_t buf[256];
    if (len > sizeof(buf)) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    ble_hs_mbuf_to_flat(ctxt->om, buf, len, &len);
    g_bleOtaService.handleDataWrite(buf, len);
    return 0;
}

static int ota_status_read_cb(uint16_t, uint16_t,
                               struct ble_gatt_access_ctxt* ctxt, void*)
{
    uint8_t buf[helix::BleOtaService::kStatusLen];
    size_t  len = sizeof(buf);
    g_bleOtaService.getStatus(buf, &len);
    return os_mbuf_append(ctxt->om, buf, static_cast<uint16_t>(len));
}

static const struct ble_gatt_svc_def kOtaGattSvcs[] = {
    {
        .type            = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid            = &kOtaSvcUuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid      = &kOtaCtrlUuid.u,
                .access_cb = ota_ctrl_write_cb,
                .flags     = BLE_GATT_CHR_F_WRITE,
            },
            {
                .uuid      = &kOtaDataUuid.u,
                .access_cb = ota_data_write_cb,
                .flags     = BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                .uuid      = &kOtaStatusUuid.u,
                .access_cb = ota_status_read_cb,
                .flags     = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            { 0 },
        },
    },
    { 0 },
};

#pragma GCC diagnostic pop

// ── NimBLE ────────────────────────────────────────────────────────────────

static void nimble_host_task(void*)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void ble_on_sync(void)
{
    ble_addr_t addr;
    ble_hs_id_infer_auto(0, &addr.type);
    ble_hs_id_copy_addr(addr.type, addr.val, nullptr);

    static const char kDevName[] = "HelixDrift-OTA";
    struct ble_hs_adv_fields fields = {};
    fields.flags            = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name             = (const uint8_t*)kDevName;
    fields.name_len         = strlen(kDevName);
    fields.name_is_complete = 1;
    ble_gap_adv_set_fields(&fields);

    struct ble_gap_adv_params adv = {};
    adv.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv.disc_mode = BLE_GAP_DISC_MODE_GEN;
    ble_gap_adv_start(addr.type, nullptr, BLE_HS_FOREVER, &adv, nullptr, nullptr);
    ESP_LOGI(TAG, "BLE advertising as '%s'", kDevName);
}

static void ble_on_reset(int reason)
{
    ESP_LOGW(TAG, "NimBLE reset reason=%d", reason);
}

// ── Public API ────────────────────────────────────────────────────────────

void board_wroom32_init(void)
{
    // NVS — required by NimBLE.
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // NimBLE + OTA GATT service.
    nimble_port_init();
    ble_hs_cfg.sync_cb  = ble_on_sync;
    ble_hs_cfg.reset_cb = ble_on_reset;
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set("HelixDrift-OTA");
    ble_gatts_count_cfg(kOtaGattSvcs);
    ble_gatts_add_svcs(kOtaGattSvcs);
    nimble_port_freertos_init(nimble_host_task);

    // Sensors — I2C scan so we can see both devices in the monitor.
    ESP_LOGI(TAG, "Scanning I2C bus (SDA=GPIO%d, SCL=GPIO%d)...",
             kI2CCfg.sdaPin, kI2CCfg.sclPin);
    for (uint8_t addr = 0x08; addr < 0x78; ++addr)
    {
        if (g_i2cBus.probe(addr))
        {
            ESP_LOGI(TAG, "  found device at 0x%02x", addr);
        }
    }

    const bool imuOk = g_imu.init();
    if (!imuOk)
    {
        ESP_LOGE(TAG, "MPU-6050 init failed — halting");
        // Sensor fusion cannot run without the IMU.
        while (true) { vTaskDelay(portMAX_DELAY); }
    }

    sf::IMagSensor* magPtr = nullptr;
    if (g_mag.init())
    {
        magPtr = &g_mag;
        ESP_LOGI(TAG, "BMM350 ready — 9-DOF mode");
    }
    else
    {
        ESP_LOGW(TAG, "BMM350 not found — 6-DOF fallback (yaw will drift)");
    }

    static helix::SensorFusionTask sfTask{g_imu, magPtr};
    g_sfTask = &sfTask;
    g_sfTask->start();

    ESP_LOGI(TAG, "Board init complete");
}

__attribute__((weak))
void wroom32_ota_confirm_image(void)
{
    const esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "ota_mark_valid failed: %d", err);
    }
}
