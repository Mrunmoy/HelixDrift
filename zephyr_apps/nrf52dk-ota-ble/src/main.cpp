#include "BleOtaService.hpp"
#include "IOtaManager.hpp"
#include "OtaManager.hpp"
#include "OtaTargetIdentity.hpp"
#include "ZephyrOtaFlashBackend.hpp"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/reboot.h>


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
helix::BleOtaService g_service{g_adapter, CONFIG_HELIX_OTA_TARGET_ID};

const gpio_dt_spec g_led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});
bool g_ledReady = false;

struct bt_conn* g_conn = nullptr;
bool g_notifyEnabled = false;

enum : uint32_t {
    DBG_EVT_BOOT = 0x01u,
    DBG_EVT_ADV_START = 0x02u,
    DBG_EVT_CONNECTED = 0x03u,
    DBG_EVT_DISCONNECTED = 0x04u,
    DBG_EVT_CTRL = 0x10u,
    DBG_EVT_DATA = 0x11u,
    DBG_EVT_REBOOT_SCHEDULED = 0x12u,
};

struct OtaDebugState {
    uint32_t magic;
    uint32_t boots;
    uint32_t event;
    uint32_t arg0;
    uint32_t arg1;
    uint32_t arg2;
    uint32_t arg3;
};

extern "C" OtaDebugState g_otaDebug __attribute__((section(".noinit")));
OtaDebugState g_otaDebug;

void updateDebug(uint32_t event, uint32_t arg0 = 0u, uint32_t arg1 = 0u, uint32_t arg2 = 0u, uint32_t arg3 = 0u) {
    g_otaDebug.magic = 0x484F5441u; // 'HOTA'
    g_otaDebug.event = event;
    g_otaDebug.arg0 = arg0;
    g_otaDebug.arg1 = arg1;
    g_otaDebug.arg2 = arg2;
    g_otaDebug.arg3 = arg3;
}

K_WORK_DELAYABLE_DEFINE(g_rebootWork, [](k_work*) { sys_reboot(SYS_REBOOT_COLD); });
void startAdvertising();
K_WORK_DELAYABLE_DEFINE(g_advRetryWork, [](k_work*) { startAdvertising(); });

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
        updateDebug(DBG_EVT_REBOOT_SCHEDULED,
                    static_cast<uint32_t>(g_manager.state()),
                    g_manager.bytesReceived(),
                    static_cast<uint32_t>(status));
        k_work_schedule(&g_rebootWork, K_MSEC(CONFIG_HELIX_OTA_REBOOT_DELAY_MS));
    }
}

ssize_t writeCtrl(struct bt_conn*, const struct bt_gatt_attr*, const void* buf, uint16_t len, uint16_t, uint8_t) {
    const auto status = g_service.handleControlWrite(static_cast<const uint8_t*>(buf), len);
    const auto* bytes = static_cast<const uint8_t*>(buf);
    const uint32_t cmd = len > 0 ? bytes[0] : 0u;
    updateDebug(DBG_EVT_CTRL,
                cmd,
                static_cast<uint32_t>(status),
                static_cast<uint32_t>(g_manager.state()),
                g_manager.bytesReceived());
    notifyStatus();
    maybeScheduleReboot(status);
    return status == helix::OtaStatus::OK ? static_cast<ssize_t>(len) : BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
}

ssize_t writeData(struct bt_conn*, const struct bt_gatt_attr*, const void* buf, uint16_t len, uint16_t, uint8_t) {
    const auto* bytes = static_cast<const uint8_t*>(buf);
    uint32_t offset = 0u;
    if (len >= 4u) {
        offset = static_cast<uint32_t>(bytes[0])
               | (static_cast<uint32_t>(bytes[1]) << 8u)
               | (static_cast<uint32_t>(bytes[2]) << 16u)
               | (static_cast<uint32_t>(bytes[3]) << 24u);
    }
    const auto status = g_service.handleDataWrite(static_cast<const uint8_t*>(buf), len);
    updateDebug(DBG_EVT_DATA,
                offset,
                static_cast<uint32_t>(len),
                static_cast<uint32_t>(status),
                g_manager.bytesReceived());
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

void connected(struct bt_conn* conn, uint8_t err) {
    if (err == 0 && !g_conn) {
        g_conn = bt_conn_ref(conn);
        k_work_cancel_delayable(&g_advRetryWork);
        updateDebug(DBG_EVT_CONNECTED, 0u, 0u, 0u, 0u);
        printk("ble: connected\n");
    }
}

void disconnected(struct bt_conn* conn, uint8_t reason) {
    ARG_UNUSED(reason);
    if (g_conn == conn) {
        bt_conn_unref(g_conn);
        g_conn = nullptr;
        g_notifyEnabled = false;
        updateDebug(DBG_EVT_DISCONNECTED, reason, 0u, 0u, 0u);
        printk("ble: disconnected\n");
        k_work_reschedule(&g_advRetryWork, K_MSEC(250));
    }
}

BT_CONN_CB_DEFINE(connCallbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

struct bt_data g_ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

const struct bt_data g_sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_HELIX_OTA_SERVICE_VAL),
};

void startAdvertising() {
    const int advErr = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, g_ad, ARRAY_SIZE(g_ad), g_sd, ARRAY_SIZE(g_sd));
    if (advErr) {
        updateDebug(DBG_EVT_ADV_START, static_cast<uint32_t>(advErr), 1u, 0u, 0u);
        printk("ble: adv start failed %d\n", advErr);
        k_work_reschedule(&g_advRetryWork, K_MSEC(500));
    } else {
        updateDebug(DBG_EVT_ADV_START, 0u, 0u, 0u, 0u);
        printk("ble: advertising as %s\n", CONFIG_BT_DEVICE_NAME);
    }
}

void heartbeat() {
    static bool on = false;
    on = !on;
    if (g_ledReady) {
        gpio_pin_set_dt(&g_led, on ? 1 : 0);
    }
}

} // namespace

int main() {
#if defined(CONFIG_BOARD_PROMICRO_NRF52840_NRF52840)
    /* NCS v3.2.4 nRF UARTE legacy shim bug: pinctrl_apply_state(DEFAULT) runs
     * in uarte_pm_resume but the PSEL.TXD register stays 0xFFFFFFFF (disconnected).
     * The new UARTE2 driver (CONFIG_UART_NRFX_UARTE_LEGACY_SHIM=n) fixes this but
     * has a gppi API mismatch on this NCS version.  Force-connect TX pin here. */
    volatile auto* pselTxd = reinterpret_cast<volatile uint32_t*>(0x40002508u);
    if (*pselTxd == 0xFFFFFFFFu) {
        *pselTxd = 9u; /* P0.09 — ProPico UART TX */
    }
#endif

    if (g_otaDebug.magic != 0x484F5441u) {
        g_otaDebug = {};
        g_otaDebug.magic = 0x484F5441u;
    }
    g_otaDebug.boots += 1u;
    updateDebug(DBG_EVT_BOOT, g_otaDebug.boots, 0u, 0u, 0u);
    printk("helix ota ble boot: %s\n", CONFIG_HELIX_OTA_LABEL);

    if (boot_write_img_confirmed() == 0) {
        printk("mcuboot: image confirmed\n");
    }

    if (!g_backend.init()) {
        printk("ota: backend init failed\n");
        return 1;
    }

    if (gpio_is_ready_dt(&g_led) && gpio_pin_configure_dt(&g_led, GPIO_OUTPUT_INACTIVE) == 0) {
        g_ledReady = true;
    }

    const int err = bt_enable(nullptr);
    if (err) {
        printk("ble: enable failed %d\n", err);
        return err;
    }

    /* Build a per-board unique name: "<base>-XXYY" from FICR DEVICEADDR */
    {
        const auto ficr0 = *reinterpret_cast<const volatile uint32_t*>(0x100000A4u);
        static const char hex[] = "0123456789ABCDEF";
        char name[28];
        const size_t n = strlen(CONFIG_BT_DEVICE_NAME);
        memcpy(name, CONFIG_BT_DEVICE_NAME, n);
        name[n]     = '-';
        name[n + 1] = hex[(ficr0 >>  4) & 0xF];
        name[n + 2] = hex[ ficr0        & 0xF];
        name[n + 3] = hex[(ficr0 >> 12) & 0xF];
        name[n + 4] = hex[(ficr0 >>  8) & 0xF];
        name[n + 5] = '\0';
        bt_set_name(name);
        /* Update advertising data to use the Zephyr-managed name buffer */
        const char* btName = bt_get_name();
        g_ad[1].data = reinterpret_cast<const uint8_t*>(btName);
        g_ad[1].data_len = static_cast<uint8_t>(strlen(btName));
        printk("ble: name %s\n", btName);
    }

    startAdvertising();

    uint32_t lastLoggedBytes = 0u;
    uint32_t lastActivityBytes = 0u;
    uint32_t stallTicks = 0u;
    constexpr uint32_t kStallTimeoutTicks = 60u; /* 60 * 500ms = 30 seconds */

    while (true) {
        heartbeat();
        const auto state = g_manager.state();
        const auto bytes = g_manager.bytesReceived();
        if (state != helix::OtaState::RECEIVING) {
            printk("tick %s state=%u bytes=%u\n",
                   CONFIG_HELIX_OTA_LABEL,
                   static_cast<unsigned>(state),
                   static_cast<unsigned>(bytes));
            stallTicks = 0u;
            lastActivityBytes = 0u;
        } else if (bytes - lastLoggedBytes >= 16384u) {
            lastLoggedBytes = bytes;
            printk("ota %s bytes=%u\n",
                   CONFIG_HELIX_OTA_LABEL,
                   static_cast<unsigned>(bytes));
        }

        /* Connection watchdog: if OTA is RECEIVING but no new data for
         * 30 seconds, the host likely crashed or walked away.  Force
         * disconnect so advertising restarts and the board can accept
         * a new OTA attempt. */
        if (g_conn && state == helix::OtaState::RECEIVING) {
            if (bytes == lastActivityBytes) {
                ++stallTicks;
            } else {
                stallTicks = 0u;
                lastActivityBytes = bytes;
            }
            if (stallTicks >= kStallTimeoutTicks) {
                printk("ota: stall detected, forcing disconnect\n");
                bt_conn_disconnect(g_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
                stallTicks = 0u;
            }
        } else {
            stallTicks = 0u;
            lastActivityBytes = 0u;
        }

        k_sleep(K_MSEC(500));
    }
}
