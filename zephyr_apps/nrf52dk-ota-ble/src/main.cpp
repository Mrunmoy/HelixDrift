#include "BleOtaService.hpp"
#include "IOtaManager.hpp"
#include "OtaManager.hpp"
#include "ZephyrOtaFlashBackend.hpp"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/reboot.h>

#include <dk_buttons_and_leds.h>

namespace {

#define BT_UUID_HELIX_OTA_SERVICE_VAL \
    BT_UUID_128_ENCODE(0x3ef6a001, 0x2d3b, 0x4f2a, 0x89e4, 0x7b59d1c0a001)
#define BT_UUID_HELIX_OTA_CTRL_VAL \
    BT_UUID_128_ENCODE(0x3ef6a002, 0x2d3b, 0x4f2a, 0x89e4, 0x7b59d1c0a001)
#define BT_UUID_HELIX_OTA_DATA_VAL \
    BT_UUID_128_ENCODE(0x3ef6a003, 0x2d3b, 0x4f2a, 0x89e4, 0x7b59d1c0a001)
#define BT_UUID_HELIX_OTA_STATUS_VAL \
    BT_UUID_128_ENCODE(0x3ef6a004, 0x2d3b, 0x4f2a, 0x89e4, 0x7b59d1c0a001)

static struct bt_uuid_128 g_serviceUuid = BT_UUID_INIT_128(BT_UUID_HELIX_OTA_SERVICE_VAL);
static struct bt_uuid_128 g_ctrlUuid = BT_UUID_INIT_128(BT_UUID_HELIX_OTA_CTRL_VAL);
static struct bt_uuid_128 g_dataUuid = BT_UUID_INIT_128(BT_UUID_HELIX_OTA_DATA_VAL);
static struct bt_uuid_128 g_statusUuid = BT_UUID_INIT_128(BT_UUID_HELIX_OTA_STATUS_VAL);

helix::ZephyrOtaFlashBackend g_backend;
helix::OtaManager g_manager{g_backend};
helix::OtaManagerAdapter g_adapter{g_manager};
helix::BleOtaService g_service{g_adapter};

struct bt_conn* g_conn = nullptr;
bool g_notifyEnabled = false;

K_WORK_DELAYABLE_DEFINE(g_rebootWork, [](k_work*) { sys_reboot(SYS_REBOOT_COLD); });

void notifyStatus() {
    if (!g_conn || !g_notifyEnabled) {
        return;
    }
    uint8_t status[helix::BleOtaService::kStatusLen] = {};
    size_t len = 0;
    g_service.getStatus(status, &len);
    bt_gatt_notify(g_conn, nullptr, status, len);
}

ssize_t readStatus(struct bt_conn*, const struct bt_gatt_attr* attr, void* buf, uint16_t len, uint16_t offset) {
    uint8_t status[helix::BleOtaService::kStatusLen] = {};
    size_t statusLen = 0;
    g_service.getStatus(status, &statusLen);
    return bt_gatt_attr_read(nullptr, attr, buf, len, offset, status, statusLen);
}

void maybeScheduleReboot(helix::OtaStatus status) {
    if (status == helix::OtaStatus::OK && g_manager.state() == helix::OtaState::COMMITTED) {
        k_work_schedule(&g_rebootWork, K_MSEC(CONFIG_HELIX_OTA_REBOOT_DELAY_MS));
    }
}

ssize_t writeCtrl(struct bt_conn*, const struct bt_gatt_attr*, const void* buf, uint16_t len, uint16_t, uint8_t) {
    const auto status = g_service.handleControlWrite(static_cast<const uint8_t*>(buf), len);
    notifyStatus();
    maybeScheduleReboot(status);
    return status == helix::OtaStatus::OK ? static_cast<ssize_t>(len) : BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
}

ssize_t writeData(struct bt_conn*, const struct bt_gatt_attr*, const void* buf, uint16_t len, uint16_t, uint8_t) {
    const auto status = g_service.handleDataWrite(static_cast<const uint8_t*>(buf), len);
    notifyStatus();
    return status == helix::OtaStatus::OK ? static_cast<ssize_t>(len) : BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
}

void statusCccChanged(const struct bt_gatt_attr*, uint16_t value) {
    g_notifyEnabled = (value == BT_GATT_CCC_NOTIFY);
}

BT_GATT_SERVICE_DEFINE(helix_ota_svc,
    BT_GATT_PRIMARY_SERVICE(&g_serviceUuid),
    BT_GATT_CHARACTERISTIC(&g_ctrlUuid.uuid,
                           BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_WRITE,
                           nullptr, writeCtrl, nullptr),
    BT_GATT_CHARACTERISTIC(&g_dataUuid.uuid,
                           BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                           BT_GATT_PERM_WRITE,
                           nullptr, writeData, nullptr),
    BT_GATT_CHARACTERISTIC(&g_statusUuid.uuid,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           readStatus, nullptr, nullptr),
    BT_GATT_CCC(statusCccChanged, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

void startAdvertising();

void connected(struct bt_conn* conn, uint8_t err) {
    if (err == 0 && !g_conn) {
        g_conn = bt_conn_ref(conn);
        printk("ble: connected\n");
    }
}

void disconnected(struct bt_conn* conn, uint8_t reason) {
    ARG_UNUSED(reason);
    if (g_conn == conn) {
        bt_conn_unref(g_conn);
        g_conn = nullptr;
        g_notifyEnabled = false;
        printk("ble: disconnected\n");
        startAdvertising();
    }
}

BT_CONN_CB_DEFINE(connCallbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

const struct bt_data g_ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

const struct bt_data g_sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_HELIX_OTA_SERVICE_VAL),
};

void startAdvertising() {
    const int advErr = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, g_ad, ARRAY_SIZE(g_ad), g_sd, ARRAY_SIZE(g_sd));
    if (advErr) {
        printk("ble: adv start failed %d\n", advErr);
    } else {
        printk("ble: advertising as %s\n", CONFIG_BT_DEVICE_NAME);
    }
}

void heartbeat() {
    static bool on = false;
    on = !on;
    if (on) {
        dk_set_led_on(DK_LED1);
    } else {
        dk_set_led_off(DK_LED1);
    }
}

} // namespace

int main() {
    printk("helix ota ble boot: %s\n", CONFIG_HELIX_OTA_LABEL);

    if (boot_write_img_confirmed() == 0) {
        printk("mcuboot: image confirmed\n");
    }

    if (!g_backend.init()) {
        printk("ota: backend init failed\n");
        return 1;
    }

    dk_leds_init();
    dk_set_led_off(DK_LED1);

    const int err = bt_enable(nullptr);
    if (err) {
        printk("ble: enable failed %d\n", err);
        return err;
    }

    startAdvertising();

    while (true) {
        heartbeat();
        printk("tick %s state=%u bytes=%u\n",
               CONFIG_HELIX_OTA_LABEL,
               static_cast<unsigned>(g_manager.state()),
               static_cast<unsigned>(g_manager.bytesReceived()));
        k_sleep(K_MSEC(500));
    }
}
