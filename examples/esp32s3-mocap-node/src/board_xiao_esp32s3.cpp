#include "board_xiao_esp32s3.h"

#include <stdint.h>
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "Esp32OtaFlashBackend.hpp"
#include "IOtaManager.hpp"
#include "BleOtaService.hpp"

static const char* TAG = "board_esp32s3";

/* ── OTA slot size from partitions_ota.csv (ota_0 / ota_1 = 960 KB) ──── */
static constexpr uint32_t kOtaSlotSize = 0xF0000u;

/* ── Forward declaration for the real EspOtaOps implementation ────────── */
class EspOtaOpsReal final : public helix::EspOtaOpsInterface {
public:
    const esp_partition_t* getNextUpdatePartition(const esp_partition_t* s) override {
        return esp_ota_get_next_update_partition(s);
    }
    esp_err_t begin(const esp_partition_t* p, size_t sz, esp_ota_handle_t* h) override {
        return esp_ota_begin(p, sz, h);
    }
    esp_err_t write(esp_ota_handle_t h, const void* d, size_t n) override {
        return esp_ota_write(h, d, n);
    }
    esp_err_t end(esp_ota_handle_t h) override {
        return esp_ota_end(h);
    }
    esp_err_t setBootPartition(const esp_partition_t* p) override {
        return esp_ota_set_boot_partition(p);
    }
};

static EspOtaOpsReal            g_otaOps;
static helix::Esp32OtaFlashBackend g_otaBackend{g_otaOps, kOtaSlotSize};
static helix::OtaManager           g_otaManager{g_otaBackend};
static helix::OtaManagerAdapter    g_otaManagerAdapter{g_otaManager};
static helix::BleOtaService        g_bleOtaService{g_otaManagerAdapter};

/* ── OTA GATT UUIDs ──────────────────────────────────────────────────── */

/* Service: 12345678-1234-1234-1234-1234567890AB */
static const ble_uuid128_t kOtaSvcUuid = BLE_UUID128_INIT(
    0xAB, 0x90, 0x78, 0x56, 0x34, 0x12, 0x34, 0x12,
    0x34, 0x12, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

/* OTA_CTRL: 12345678-1234-1234-1234-1234567890AC */
static const ble_uuid128_t kOtaCtrlUuid = BLE_UUID128_INIT(
    0xAC, 0x90, 0x78, 0x56, 0x34, 0x12, 0x34, 0x12,
    0x34, 0x12, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

/* OTA_DATA: 12345678-1234-1234-1234-1234567890AD */
static const ble_uuid128_t kOtaDataUuid = BLE_UUID128_INIT(
    0xAD, 0x90, 0x78, 0x56, 0x34, 0x12, 0x34, 0x12,
    0x34, 0x12, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

/* OTA_STATUS: 12345678-1234-1234-1234-1234567890AE */
static const ble_uuid128_t kOtaStatusUuid = BLE_UUID128_INIT(
    0xAE, 0x90, 0x78, 0x56, 0x34, 0x12, 0x34, 0x12,
    0x34, 0x12, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

/* ── GATT attribute callbacks ─────────────────────────────────────────── */

static int ota_ctrl_write_cb(uint16_t /*conn*/, uint16_t /*attr*/,
                              struct ble_gatt_access_ctxt* ctxt, void* /*arg*/) {
    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    uint8_t  buf[64];
    if (len > sizeof(buf)) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    ble_hs_mbuf_to_flat(ctxt->om, buf, len, &len);
    g_bleOtaService.handleControlWrite(buf, len);
    return 0;
}

static int ota_data_write_cb(uint16_t /*conn*/, uint16_t /*attr*/,
                              struct ble_gatt_access_ctxt* ctxt, void* /*arg*/) {
    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    /* Up to 512 bytes (BLE 5.0 max ATT payload). */
    uint8_t buf[512];
    if (len > sizeof(buf)) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    ble_hs_mbuf_to_flat(ctxt->om, buf, len, &len);
    g_bleOtaService.handleDataWrite(buf, len);
    return 0;
}

static int ota_status_read_cb(uint16_t /*conn*/, uint16_t /*attr*/,
                               struct ble_gatt_access_ctxt* ctxt, void* /*arg*/) {
    uint8_t buf[helix::BleOtaService::kStatusLen];
    size_t  len = sizeof(buf);
    g_bleOtaService.getStatus(buf, &len);
    return os_mbuf_append(ctxt->om, buf, static_cast<uint16_t>(len));
}

/* ── GATT service table ───────────────────────────────────────────────── */

static const struct ble_gatt_svc_def kOtaGattSvcs[] = {
    {
        .type       = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid       = &kOtaSvcUuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {   /* OTA_CTRL */
                .uuid        = &kOtaCtrlUuid.u,
                .access_cb   = ota_ctrl_write_cb,
                .flags       = BLE_GATT_CHR_F_WRITE,
            },
            {   /* OTA_DATA */
                .uuid        = &kOtaDataUuid.u,
                .access_cb   = ota_data_write_cb,
                .flags       = BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {   /* OTA_STATUS */
                .uuid        = &kOtaStatusUuid.u,
                .access_cb   = ota_status_read_cb,
                .flags       = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            { 0 }, /* sentinel */
        },
    },
    { 0 }, /* sentinel */
};

/* ── NimBLE host task ─────────────────────────────────────────────────── */

static void nimble_host_task(void* param) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void ble_on_sync(void) {
    ble_addr_t addr;
    ble_hs_id_infer_auto(0, &addr.type);
    ble_hs_id_copy_addr(addr.type, addr.val, nullptr);

    const struct ble_gap_adv_params adv_params = {
        .conn_mode = BLE_GAP_CONN_MODE_UND,
        .disc_mode = BLE_GAP_DISC_MODE_GEN,
    };
    ble_gap_adv_start(addr.type, nullptr, BLE_HS_FOREVER, &adv_params,
                      nullptr, nullptr);
    ESP_LOGI(TAG, "BLE advertising started");
}

static void ble_on_reset(int reason) {
    ESP_LOGW(TAG, "NimBLE reset, reason=%d", reason);
}

/* ── Public API ───────────────────────────────────────────────────────── */

void board_xiao_esp32s3_init(void) {
    nimble_port_init();

    ble_hs_cfg.sync_cb  = ble_on_sync;
    ble_hs_cfg.reset_cb = ble_on_reset;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set("HelixDrift-OTA");

    ble_gatts_count_cfg(kOtaGattSvcs);
    ble_gatts_add_svcs(kOtaGattSvcs);

    nimble_port_freertos_init(nimble_host_task);
    ESP_LOGI(TAG, "Board init done (OTA GATT service registered)");
}

__attribute__((weak))
void xiao_ota_confirm_image(void) {
    const esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ota_mark_valid failed: %d", err);
    }
}
