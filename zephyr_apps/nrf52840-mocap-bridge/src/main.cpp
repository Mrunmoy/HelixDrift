#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/types.h>
#include <string.h>

#include <esb.h>

#include "usbd_init.h"

#if defined(CONFIG_BT) && defined(CONFIG_HELIX_MOCAP_BRIDGE_ROLE_NODE)
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/dfu/mcuboot.h>

#include "BleOtaService.hpp"
#include "IOtaManager.hpp"
#include "OtaManager.hpp"
#include "OtaTargetIdentity.hpp"
#include "ZephyrOtaFlashBackend.hpp"
#endif

LOG_MODULE_REGISTER(helix_mocap_bridge, LOG_LEVEL_INF);

enum {
	HELIX_PACKET_SYNC_ANCHOR = 0xA1,
	HELIX_PACKET_MOCAP_FRAME = 0xC1,
};

struct __packed HelixSyncAnchor {
	uint8_t type;
	uint8_t central_id;
	uint8_t anchor_sequence;
	uint8_t session_tag;
	uint32_t central_timestamp_us;
};

struct __packed HelixMocapFrame {
	uint8_t type;
	uint8_t node_id;
	uint8_t sequence;
	uint8_t session_tag;
	uint32_t node_local_timestamp_us;
	uint32_t node_synced_timestamp_us;
	int16_t yaw_cdeg;
	int16_t pitch_cdeg;
	int16_t roll_cdeg;
	int16_t x_mm;
	int16_t y_mm;
	int16_t z_mm;
};

struct HelixMocapStatus {
	uint32_t magic;
	uint32_t role;
	uint32_t heartbeat;
	uint32_t phase;
	uint32_t tx_attempts;
	uint32_t tx_success;
	uint32_t tx_failed;
	uint32_t rx_packets;
	uint32_t anchors_sent;
	uint32_t anchors_received;
	uint32_t usb_lines;
	uint32_t tracked_nodes;
	int32_t estimated_offset_us;
	uint32_t last_node_id;
	uint32_t last_sequence;
	uint32_t last_rx_len;
	uint32_t last_error;
};

struct NodeTrack {
	uint8_t node_id;
	bool active;
	uint32_t packets;
	uint32_t gaps;
	uint8_t last_sequence;
	uint32_t last_node_timestamp_us;
	uint32_t last_synced_timestamp_us;
	uint32_t last_rx_timestamp_us;
};

volatile struct HelixMocapStatus g_helixMocapStatus;

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});
static const struct device *const host_stream = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
static struct esb_payload rx_payload;
#if defined(CONFIG_HELIX_MOCAP_BRIDGE_ROLE_NODE)
static struct esb_payload tx_payload;
static atomic_t tx_ready = ATOMIC_INIT(1);
static bool have_anchor;
static int32_t estimated_offset_us;
#endif
#if defined(CONFIG_HELIX_MOCAP_BRIDGE_ROLE_CENTRAL)
static struct NodeTrack tracked_nodes[CONFIG_HELIX_MOCAP_MAX_TRACKED_NODES];
static uint8_t next_anchor_sequence;
#endif

static uint32_t now_us(void)
{
	return (uint32_t)(k_uptime_get() * 1000LL);
}

static void status_set_error(int err)
{
	g_helixMocapStatus.last_error = (uint32_t)(err < 0 ? -err : err);
}

static int clocks_start(void)
{
	int err;
	int res;
	struct onoff_manager *clk_mgr;
	struct onoff_client clk_cli;

	clk_mgr = z_nrf_clock_control_get_onoff(CLOCK_CONTROL_NRF_SUBSYS_HF);
	if (clk_mgr == NULL) {
		return -ENXIO;
	}

	sys_notify_init_spinwait(&clk_cli.notify);
	err = onoff_request(clk_mgr, &clk_cli);
	if (err < 0) {
		return err;
	}

	do {
		err = sys_notify_fetch_result(&clk_cli.notify, &res);
		if (!err && res) {
			return res;
		}
	} while (err);

	return 0;
}

static void led_set(bool on)
{
	if (!gpio_is_ready_dt(&led)) {
		return;
	}

	(void)gpio_pin_set_dt(&led, on ? 1 : 0);
}

static void heartbeat_tick(void)
{
	static bool on;

#if defined(CONFIG_HELIX_MOCAP_BRIDGE_ROLE_NODE) && defined(CONFIG_HELIX_MOCAP_NODE_LED_DEBUG)
	uint32_t period_ms = 125U;

	if (have_anchor) {
		period_ms = 500U;
	} else if (g_helixMocapStatus.tx_success > 0U) {
		period_ms = 250U;
	}

	led_set(((now_us() / 1000U) / period_ms) & 1U);
#else
	on = !on;
	led_set(on);
#endif
	g_helixMocapStatus.heartbeat++;
}

static void host_write(const char *line)
{
	if (!device_is_ready(host_stream)) {
		return;
	}

	for (const char *p = line; *p != '\0'; ++p) {
		uart_poll_out(host_stream, (unsigned char)*p);
	}
}

static void report_summary(void)
{
	char line[192];

#if defined(CONFIG_HELIX_MOCAP_BRIDGE_ROLE_CENTRAL)
	snprintk(line,
		 sizeof(line),
		 "SUMMARY role=central rx=%u anchors=%u tracked=%u usb_lines=%u err=%u hb=%u\n",
		 g_helixMocapStatus.rx_packets,
		 g_helixMocapStatus.anchors_sent,
		 g_helixMocapStatus.tracked_nodes,
		 g_helixMocapStatus.usb_lines,
		 g_helixMocapStatus.last_error,
		 g_helixMocapStatus.heartbeat);
#else
	snprintk(line,
		 sizeof(line),
		 "SUMMARY role=node id=%u tx_ok=%u tx_fail=%u anchors=%u offset_us=%d err=%u hb=%u\n",
		 CONFIG_HELIX_MOCAP_NODE_ID,
		 g_helixMocapStatus.tx_success,
		 g_helixMocapStatus.tx_failed,
		 g_helixMocapStatus.anchors_received,
		 g_helixMocapStatus.estimated_offset_us,
		 g_helixMocapStatus.last_error,
		 g_helixMocapStatus.heartbeat);
#endif

	host_write(line);
}

#if defined(CONFIG_USB_DEVICE_STACK) || defined(CONFIG_USB_DEVICE_STACK_NEXT)
static int host_stream_init(void)
{
	if (!device_is_ready(host_stream)) {
		return -ENODEV;
	}

	return helix_usbd_enable(host_stream, CONFIG_HELIX_MOCAP_USB_WAIT_FOR_DTR_MS);
}
#else
static int host_stream_init(void)
{
	return device_is_ready(host_stream) ? 0 : -ENODEV;
}
#endif

#if defined(CONFIG_HELIX_MOCAP_BRIDGE_ROLE_CENTRAL)
static struct NodeTrack *find_or_add_track(uint8_t node_id)
{
	struct NodeTrack *free_slot = NULL;

	for (size_t i = 0; i < ARRAY_SIZE(tracked_nodes); ++i) {
		if (tracked_nodes[i].active && tracked_nodes[i].node_id == node_id) {
			return &tracked_nodes[i];
		}
		if (!tracked_nodes[i].active && free_slot == NULL) {
			free_slot = &tracked_nodes[i];
		}
	}

	if (free_slot == NULL) {
		return NULL;
	}

	free_slot->active = true;
	free_slot->node_id = node_id;
	g_helixMocapStatus.tracked_nodes++;
	return free_slot;
}

static void host_emit_frame(const struct HelixMocapFrame *frame, uint32_t rx_timestamp_us,
			    uint32_t gaps)
{
	char line[224];

	snprintk(line,
		 sizeof(line),
		 "FRAME node=%u seq=%u node_us=%u sync_us=%u rx_us=%u yaw_cd=%d pitch_cd=%d roll_cd=%d x_mm=%d y_mm=%d z_mm=%d gaps=%u\n",
		 frame->node_id,
		 frame->sequence,
		 frame->node_local_timestamp_us,
		 frame->node_synced_timestamp_us,
		 rx_timestamp_us,
		 frame->yaw_cdeg,
		 frame->pitch_cdeg,
		 frame->roll_cdeg,
		 frame->x_mm,
		 frame->y_mm,
		 frame->z_mm,
		 gaps);
	host_write(line);
	g_helixMocapStatus.usb_lines++;
}

static void central_handle_frame(const struct esb_payload *payload)
{
	const struct HelixMocapFrame *frame = (const struct HelixMocapFrame *)payload->data;
	struct NodeTrack *track;
	uint32_t rx_timestamp_us = now_us();
	uint32_t gaps = 0U;

	if (payload->length < sizeof(*frame) ||
	    frame->type != HELIX_PACKET_MOCAP_FRAME ||
	    frame->session_tag != (uint8_t)CONFIG_HELIX_MOCAP_SESSION_TAG) {
		return;
	}

	track = find_or_add_track(frame->node_id);
	if (track == NULL) {
		return;
	}

	if (track->packets > 0U) {
		const uint8_t expected = (uint8_t)(track->last_sequence + 1U);
		if (frame->sequence != expected) {
			gaps = (uint8_t)(frame->sequence - expected);
			track->gaps += gaps;
		}
	}

	track->packets++;
	track->last_sequence = frame->sequence;
	track->last_node_timestamp_us = frame->node_local_timestamp_us;
	track->last_synced_timestamp_us = frame->node_synced_timestamp_us;
	track->last_rx_timestamp_us = rx_timestamp_us;

	g_helixMocapStatus.last_node_id = frame->node_id;
	g_helixMocapStatus.last_sequence = frame->sequence;

	host_emit_frame(frame, rx_timestamp_us, gaps);

	struct HelixSyncAnchor anchor = {
		.type = HELIX_PACKET_SYNC_ANCHOR,
		.central_id = (uint8_t)CONFIG_HELIX_MOCAP_CENTRAL_ID,
		.anchor_sequence = next_anchor_sequence++,
		.session_tag = (uint8_t)CONFIG_HELIX_MOCAP_SESSION_TAG,
		.central_timestamp_us = rx_timestamp_us,
	};
	struct esb_payload ack = {
		.pipe = payload->pipe,
		.length = sizeof(anchor),
	};

	memcpy(ack.data, &anchor, sizeof(anchor));
	if (esb_write_payload(&ack) == 0) {
		g_helixMocapStatus.anchors_sent++;
	}
}
#endif

#if defined(CONFIG_HELIX_MOCAP_BRIDGE_ROLE_NODE)
static int16_t synth_wave(uint8_t sequence, int32_t mul, int32_t span)
{
	int32_t phase = ((int32_t)sequence * mul) % (span * 2);

	if (phase > span) {
		phase = (span * 2) - phase;
	}

	return (int16_t)(phase - (span / 2));
}

static void node_fill_frame(struct HelixMocapFrame *frame)
{
	const uint8_t sequence = (uint8_t)(g_helixMocapStatus.tx_attempts & 0xFF);
	const uint32_t local_us = now_us();
	const uint32_t synced_us = local_us - (uint32_t)estimated_offset_us;

	frame->type = HELIX_PACKET_MOCAP_FRAME;
	frame->node_id = (uint8_t)CONFIG_HELIX_MOCAP_NODE_ID;
	frame->sequence = sequence;
	frame->session_tag = (uint8_t)CONFIG_HELIX_MOCAP_SESSION_TAG;
	frame->node_local_timestamp_us = local_us;
	frame->node_synced_timestamp_us = synced_us;
	frame->yaw_cdeg = synth_wave(sequence, 37, 3600);
	frame->pitch_cdeg = synth_wave(sequence, 23, 1800);
	frame->roll_cdeg = synth_wave(sequence, 17, 1800);
	frame->x_mm = synth_wave(sequence, 11, 1200);
	frame->y_mm = synth_wave(sequence, 13, 1200);
	frame->z_mm = synth_wave(sequence, 19, 1200);
}

static void node_handle_anchor(const struct esb_payload *payload)
{
	const struct HelixSyncAnchor *anchor = (const struct HelixSyncAnchor *)payload->data;
	const uint32_t local_us = now_us();

	if (payload->length < sizeof(*anchor) ||
	    anchor->type != HELIX_PACKET_SYNC_ANCHOR ||
	    anchor->session_tag != (uint8_t)CONFIG_HELIX_MOCAP_SESSION_TAG) {
		return;
	}

	estimated_offset_us = (int32_t)(local_us - anchor->central_timestamp_us);
	g_helixMocapStatus.estimated_offset_us = estimated_offset_us;
	g_helixMocapStatus.anchors_received++;
	have_anchor = true;
}
#endif

static void event_handler(struct esb_evt const *event)
{
	switch (event->evt_id) {
	case ESB_EVENT_TX_SUCCESS:
		g_helixMocapStatus.tx_success++;
#if defined(CONFIG_HELIX_MOCAP_BRIDGE_ROLE_NODE)
		atomic_set(&tx_ready, 1);
#endif
		break;
	case ESB_EVENT_TX_FAILED:
		g_helixMocapStatus.tx_failed++;
		(void)esb_flush_tx();
#if defined(CONFIG_HELIX_MOCAP_BRIDGE_ROLE_NODE)
		atomic_set(&tx_ready, 1);
#endif
		break;
	case ESB_EVENT_RX_RECEIVED:
		while (esb_read_rx_payload(&rx_payload) == 0) {
			g_helixMocapStatus.rx_packets++;
			g_helixMocapStatus.last_rx_len = rx_payload.length;
#if defined(CONFIG_HELIX_MOCAP_BRIDGE_ROLE_CENTRAL)
			central_handle_frame(&rx_payload);
#else
			node_handle_anchor(&rx_payload);
#endif
		}
		break;
	default:
		break;
	}
}

static int esb_initialize(void)
{
	int err;
	uint8_t base_addr_0[4] = {0xD3, 0xC1, 0xB2, 0xA1};
	uint8_t base_addr_1[4] = {0x44, 0x33, 0x22, 0x11};
	uint8_t addr_prefix[8] = {0xE7, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8};
	struct esb_config config = ESB_DEFAULT_CONFIG;

	config.protocol = ESB_PROTOCOL_ESB_DPL;
	config.bitrate = ESB_BITRATE_2MBPS;
	config.event_handler = event_handler;
	config.retransmit_delay = 600;
	config.retransmit_count = 6;
	config.selective_auto_ack = true;
	config.payload_length = 32;
	config.mode =
#if defined(CONFIG_HELIX_MOCAP_BRIDGE_ROLE_CENTRAL)
		ESB_MODE_PRX;
#else
		ESB_MODE_PTX;
#endif

	err = esb_init(&config);
	if (err) {
		return err;
	}

	err = esb_set_base_address_0(base_addr_0);
	if (err) {
		return err;
	}

	err = esb_set_base_address_1(base_addr_1);
	if (err) {
		return err;
	}

	err = esb_set_prefixes(addr_prefix, ARRAY_SIZE(addr_prefix));
	if (err) {
		return err;
	}

	err = esb_set_rf_channel(CONFIG_HELIX_MOCAP_RF_CHANNEL);
	if (err) {
		return err;
	}

	(void)esb_flush_tx();
	(void)esb_flush_rx();
	return 0;
}

#if defined(CONFIG_HELIX_MOCAP_BRIDGE_ROLE_NODE)
static void maybe_send_frame(void)
{
	struct HelixMocapFrame frame;
	int err;

	if (!atomic_cas(&tx_ready, 1, 0)) {
		return;
	}

	node_fill_frame(&frame);
	tx_payload.pipe = CONFIG_HELIX_MOCAP_PIPE;
	tx_payload.noack = 0;
	tx_payload.length = sizeof(frame);
	memcpy(tx_payload.data, &frame, sizeof(frame));

	g_helixMocapStatus.last_sequence = frame.sequence;
	g_helixMocapStatus.last_node_id = frame.node_id;
	g_helixMocapStatus.tx_attempts++;

	err = esb_write_payload(&tx_payload);
	if (err) {
		status_set_error(err);
		atomic_set(&tx_ready, 1);
	}
}
#endif

/* ── BLE OTA Boot Window (node role only) ──────────────────────── */
#if defined(CONFIG_BT) && defined(CONFIG_HELIX_MOCAP_BRIDGE_ROLE_NODE) && \
    (CONFIG_HELIX_OTA_BOOT_WINDOW_MS > 0)

#define BT_UUID_HELIX_OTA_SERVICE_VAL \
    BT_UUID_128_ENCODE(0x3ef6a001, 0x2d3b, 0x4f2a, 0x89e4, 0x7b59d1c0a001)
#define BT_UUID_HELIX_OTA_CTRL_VAL \
    BT_UUID_128_ENCODE(0x3ef6a002, 0x2d3b, 0x4f2a, 0x89e4, 0x7b59d1c0a001)
#define BT_UUID_HELIX_OTA_DATA_VAL \
    BT_UUID_128_ENCODE(0x3ef6a003, 0x2d3b, 0x4f2a, 0x89e4, 0x7b59d1c0a001)
#define BT_UUID_HELIX_OTA_STATUS_VAL \
    BT_UUID_128_ENCODE(0x3ef6a004, 0x2d3b, 0x4f2a, 0x89e4, 0x7b59d1c0a001)

static struct bt_uuid_128 ota_svc_uuid = BT_UUID_INIT_128(BT_UUID_HELIX_OTA_SERVICE_VAL);
static struct bt_uuid_128 ota_ctrl_uuid = BT_UUID_INIT_128(BT_UUID_HELIX_OTA_CTRL_VAL);
static struct bt_uuid_128 ota_data_uuid = BT_UUID_INIT_128(BT_UUID_HELIX_OTA_DATA_VAL);
static struct bt_uuid_128 ota_status_uuid = BT_UUID_INIT_128(BT_UUID_HELIX_OTA_STATUS_VAL);

static helix::ZephyrOtaFlashBackend ota_backend;
static helix::OtaManager ota_manager{ota_backend};
static helix::OtaManagerAdapter ota_adapter{ota_manager};
static helix::BleOtaService ota_service{ota_adapter, CONFIG_HELIX_OTA_TARGET_ID};

static struct bt_conn *ota_conn;
static bool ota_connected;

static ssize_t ota_write_ctrl(struct bt_conn *, const struct bt_gatt_attr *,
                              const void *buf, uint16_t len, uint16_t, uint8_t)
{
	auto status = ota_service.handleControlWrite(static_cast<const uint8_t *>(buf), len);
	if (status == helix::OtaStatus::OK &&
	    ota_manager.state() == helix::OtaState::COMMITTED) {
		printk("ota: committed, rebooting\n");
		k_sleep(K_MSEC(CONFIG_HELIX_OTA_REBOOT_DELAY_MS));
		sys_reboot(SYS_REBOOT_COLD);
	}
	return status == helix::OtaStatus::OK
	       ? static_cast<ssize_t>(len)
	       : BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
}

static ssize_t ota_write_data(struct bt_conn *, const struct bt_gatt_attr *,
                              const void *buf, uint16_t len, uint16_t, uint8_t)
{
	auto status = ota_service.handleDataWrite(static_cast<const uint8_t *>(buf), len);
	return status == helix::OtaStatus::OK
	       ? static_cast<ssize_t>(len)
	       : BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
}

static ssize_t ota_read_status(struct bt_conn *, const struct bt_gatt_attr *attr,
                               void *buf, uint16_t len, uint16_t offset)
{
	uint8_t status[helix::BleOtaService::kStatusLen] = {};
	size_t slen = 0;
	ota_service.getStatus(status, &slen);
	return bt_gatt_attr_read(nullptr, attr, buf, len, offset, status, slen);
}

BT_GATT_SERVICE_DEFINE(helix_ota_svc,
    BT_GATT_PRIMARY_SERVICE(&ota_svc_uuid),
    BT_GATT_CHARACTERISTIC(&ota_ctrl_uuid.uuid,
                           BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_WRITE,
                           nullptr, ota_write_ctrl, nullptr),
    BT_GATT_CHARACTERISTIC(&ota_data_uuid.uuid,
                           BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                           BT_GATT_PERM_WRITE,
                           nullptr, ota_write_data, nullptr),
    BT_GATT_CHARACTERISTIC(&ota_status_uuid.uuid,
                           BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ,
                           ota_read_status, nullptr, nullptr),
);

static void ota_bt_connected(struct bt_conn *conn, uint8_t err)
{
	if (err == 0 && !ota_conn) {
		ota_conn = bt_conn_ref(conn);
		ota_connected = true;
		printk("ota: connected\n");
	}
}

static void ota_bt_disconnected(struct bt_conn *conn, uint8_t reason)
{
	if (ota_conn == conn) {
		bt_conn_unref(ota_conn);
		ota_conn = nullptr;
		ota_connected = false;
		printk("ota: disconnected reason=%u\n", reason);
	}
}

BT_CONN_CB_DEFINE(ota_conn_cbs) = {
    .connected = ota_bt_connected,
    .disconnected = ota_bt_disconnected,
};

/* Run the BLE OTA boot window. Returns true if OTA completed (reboot
 * happens inside), false if the window expired with no connection. */
static bool run_ota_boot_window(void)
{
	printk("ota: boot window %d ms\n", CONFIG_HELIX_OTA_BOOT_WINDOW_MS);
	g_helixMocapStatus.last_error = 0xBB01U; /* entered OTA window */

	if (boot_write_img_confirmed() == 0) {
		printk("mcuboot: image confirmed\n");
	}
	g_helixMocapStatus.last_error = 0xBB02U; /* img confirmed */

	if (!ota_backend.init()) {
		printk("ota: backend init failed\n");
		g_helixMocapStatus.last_error = 0xBBFEU;
		return false;
	}
	g_helixMocapStatus.last_error = 0xBB03U; /* backend init ok */

	int err = bt_enable(nullptr);
	if (err) {
		printk("ota: bt_enable failed %d\n", err);
		g_helixMocapStatus.last_error = 0xBB00U | (uint32_t)(unsigned)(-err);
		return false;
	}
	g_helixMocapStatus.last_error = 0xBB04U; /* bt_enable ok */

	/* Build unique name from FICR */
	{
		const auto ficr0 = *reinterpret_cast<const volatile uint32_t *>(0x100000A4U);
		static const char hex[] = "0123456789ABCDEF";
		char name[16];
		const char *base = CONFIG_BT_DEVICE_NAME;
		size_t n = strlen(base);
		memcpy(name, base, n);
		name[n]     = '-';
		name[n + 1] = hex[(ficr0 >>  4) & 0xF];
		name[n + 2] = hex[ ficr0        & 0xF];
		name[n + 3] = hex[(ficr0 >> 12) & 0xF];
		name[n + 4] = hex[(ficr0 >>  8) & 0xF];
		name[n + 5] = '\0';
		bt_set_name(name);
		printk("ota: name %s\n", name);
	}

	/* Build advertising data — use static storage so pointers persist */
	const char *bt_name = bt_get_name();
	static uint8_t adv_flags[] = {BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR};
	static struct bt_data ad[2];
	ad[0] = {BT_DATA_FLAGS, sizeof(adv_flags), adv_flags};
	ad[1] = {BT_DATA_NAME_COMPLETE, static_cast<uint8_t>(strlen(bt_name)),
	         reinterpret_cast<const uint8_t *>(bt_name)};
	static const struct bt_data sd[] = {
		BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_HELIX_OTA_SERVICE_VAL),
	};

	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, 2, sd, ARRAY_SIZE(sd));
	if (err) {
		printk("ota: adv start failed %d\n", err);
		g_helixMocapStatus.last_error = 0xBBA0U | (uint32_t)(unsigned)(-err);
		bt_disable();
		return false;
	}
	g_helixMocapStatus.last_error = 0xBB05U; /* adv start ok */

	/* Wait for connection or timeout */
	int64_t deadline = k_uptime_get() + CONFIG_HELIX_OTA_BOOT_WINDOW_MS;
	while (k_uptime_get() < deadline && !ota_connected) {
		led_set(((k_uptime_get() / 100) & 1) != 0); /* fast blink = OTA window */
		k_sleep(K_MSEC(50));
	}

	if (!ota_connected) {
		printk("ota: window expired, switching to ESB\n");
		g_helixMocapStatus.last_error = 0xBB06U; /* window expired */
		bt_le_adv_stop();
		bt_disable();
		return false;
	}

	/* Connected — run OTA until complete or disconnect.
	 * OTA commit triggers reboot inside ota_write_ctrl().
	 * Stall watchdog: if no data for 30s, force disconnect. */
	printk("ota: running OTA transfer\n");
	uint32_t last_bytes = 0;
	uint32_t stall_ticks = 0;
	while (ota_connected) {
		uint32_t bytes = ota_manager.bytesReceived();
		if (ota_manager.state() == helix::OtaState::RECEIVING) {
			if (bytes == last_bytes) {
				stall_ticks++;
			} else {
				stall_ticks = 0;
				last_bytes = bytes;
			}
			if (stall_ticks >= 600) { /* 600 * 50ms = 30s */
				printk("ota: stall, disconnecting\n");
				bt_conn_disconnect(ota_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
				break;
			}
		}
		led_set(((k_uptime_get() / 250) & 1) != 0); /* medium blink = OTA active */
		k_sleep(K_MSEC(50));
	}

	/* If we get here without reboot, OTA was aborted or stalled */
	printk("ota: session ended without commit, switching to ESB\n");
	bt_le_adv_stop();
	bt_disable();
	return false;
}

#endif /* CONFIG_BT && ROLE_NODE && BOOT_WINDOW > 0 */

int main(void)
{
	int err;

	memset((void *)&g_helixMocapStatus, 0, sizeof(g_helixMocapStatus));
	g_helixMocapStatus.magic = 0x484D4244U;
	g_helixMocapStatus.role =
#if defined(CONFIG_HELIX_MOCAP_BRIDGE_ROLE_CENTRAL)
		1U;
#else
		2U;
#endif
	g_helixMocapStatus.phase = 1U;

	if (gpio_is_ready_dt(&led)) {
		(void)gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	}

	err = clocks_start();
	if (err) {
		status_set_error(err);
	}
	g_helixMocapStatus.phase = 2U;

	err = host_stream_init();
	if (err) {
		status_set_error(err);
	}
	g_helixMocapStatus.phase = 3U;

#if defined(CONFIG_BOARD_PROMICRO_NRF52840_NRF52840)
	/* NCS v3.2.4 UARTE legacy shim PSEL.TXD workaround */
	{
		volatile auto *pselTxd = reinterpret_cast<volatile uint32_t *>(0x40002508U);
		if (*pselTxd == 0xFFFFFFFFU) {
			*pselTxd = 9U;
		}
	}
#endif

	/* ── BLE OTA boot window (node only) ────────────────── */
#if defined(CONFIG_BT) && defined(CONFIG_HELIX_MOCAP_BRIDGE_ROLE_NODE) && \
    (CONFIG_HELIX_OTA_BOOT_WINDOW_MS > 0)
	run_ota_boot_window();
	/* If we return here, OTA either wasn't requested or was aborted.
	 * bt_disable() was called. Proceed to ESB. */
#endif

	/* ── ESB mocap mode ─────────────────────────────────── */
	err = esb_initialize();
	if (err) {
		status_set_error(err);
	}
	g_helixMocapStatus.phase = 4U;

#if defined(CONFIG_HELIX_MOCAP_BRIDGE_ROLE_CENTRAL)
	err = esb_start_rx();
	if (err) {
		status_set_error(err);
	}
#endif

	host_write("HELIX_MOCAP_BRIDGE_READY\n");

	while (1) {
		heartbeat_tick();
#if defined(CONFIG_HELIX_MOCAP_BRIDGE_ROLE_NODE)
		maybe_send_frame();
#endif
		if ((g_helixMocapStatus.heartbeat % 25U) == 0U) {
			report_summary();
		}
		k_sleep(K_MSEC(CONFIG_HELIX_MOCAP_SEND_PERIOD_MS));
	}

	return 0;
}
