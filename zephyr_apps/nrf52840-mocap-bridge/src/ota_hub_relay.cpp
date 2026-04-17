#include "ota_hub_relay.hpp"

#if defined(CONFIG_HELIX_OTA_HUB_RELAY)

#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/ring_buffer.h>
#include <string.h>

extern "C" {
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
}

#include "UartOtaProtocol.hpp"

using helix::UartOtaProtocol;
using helix::UartOtaMutableFrame;
using helix::UartOtaFrameParser;

/* ── USB CDC RX ────────────────────────────────────────────────── */

RING_BUF_DECLARE(usb_rx_rb, 1024);
static const struct device *host_dev;
static UartOtaFrameParser<512> parser;

static void usb_rx_isr(const struct device *dev, void *user_data)
{
	uart_irq_update(dev);
	while (uart_irq_rx_ready(dev)) {
		uint8_t buf[64];
		int n = uart_fifo_read(dev, buf, sizeof(buf));
		if (n > 0) {
			ring_buf_put(&usb_rx_rb, buf, (uint32_t)n);
		}
	}
}

static void host_write_bytes(const uint8_t *data, size_t len)
{
	if (!device_is_ready(host_dev)) {
		return;
	}
	for (size_t i = 0; i < len; ++i) {
		uart_poll_out(host_dev, data[i]);
	}
}

static void send_response(uint8_t type, const uint8_t *payload, size_t payloadLen)
{
	uint8_t buf[128];
	size_t outLen = 0;
	if (UartOtaProtocol::encode(type, payload, payloadLen, buf, sizeof(buf), outLen)) {
		host_write_bytes(buf, outLen);
	}
}

/* ── BLE GATT client ───────────────────────────────────────────── */

#define BT_UUID_HELIX_OTA_SVC   BT_UUID_128_ENCODE(0x3ef6a001, 0x2d3b, 0x4f2a, 0x89e4, 0x7b59d1c0a001)
#define BT_UUID_HELIX_OTA_CTRL  BT_UUID_128_ENCODE(0x3ef6a002, 0x2d3b, 0x4f2a, 0x89e4, 0x7b59d1c0a001)
#define BT_UUID_HELIX_OTA_DATA  BT_UUID_128_ENCODE(0x3ef6a003, 0x2d3b, 0x4f2a, 0x89e4, 0x7b59d1c0a001)
#define BT_UUID_HELIX_OTA_STAT  BT_UUID_128_ENCODE(0x3ef6a004, 0x2d3b, 0x4f2a, 0x89e4, 0x7b59d1c0a001)

static struct bt_uuid_128 ota_svc_uuid  = BT_UUID_INIT_128(BT_UUID_HELIX_OTA_SVC);
static struct bt_uuid_128 ota_ctrl_uuid = BT_UUID_INIT_128(BT_UUID_HELIX_OTA_CTRL);
static struct bt_uuid_128 ota_data_uuid = BT_UUID_INIT_128(BT_UUID_HELIX_OTA_DATA);
static struct bt_uuid_128 ota_stat_uuid = BT_UUID_INIT_128(BT_UUID_HELIX_OTA_STAT);

static struct bt_conn *relay_conn;
static uint16_t gatt_ctrl_handle;
static uint16_t gatt_data_handle;
static uint16_t gatt_stat_handle;

enum class RelayState : uint8_t {
	IDLE,
	BLE_INIT,
	SCANNING,
	CONNECTING,
	DISCOVERING,
	READY,
	DONE,
	ERR,
};

static volatile RelayState relay_state = RelayState::IDLE;
static char target_name[16];
static volatile bool gatt_write_done;
static volatile int gatt_write_err;
static volatile bool gatt_read_done;
static uint8_t gatt_read_buf[32];
static volatile size_t gatt_read_len;

/* ── Scan callback ─────────────────────────────────────────────── */

static void scan_recv_cb(const struct bt_le_scan_recv_info *info,
                         struct net_buf_simple *buf)
{
	/* Parse advertising data for local name */
	while (buf->len > 1) {
		uint8_t len = net_buf_simple_pull_u8(buf);
		if (len == 0 || len > buf->len) {
			break;
		}
		uint8_t type = net_buf_simple_pull_u8(buf);
		len--;
		if ((type == BT_DATA_NAME_COMPLETE || type == BT_DATA_NAME_SHORTENED) &&
		    len > 0 && len < sizeof(target_name)) {
			char name[16] = {};
			memcpy(name, buf->data, len);
			name[len] = '\0';
			if (strncmp(name, target_name, strlen(target_name)) == 0) {
				printk("relay: found %s\n", name);
				bt_le_scan_stop();
				static const struct bt_conn_le_create_param create_param = {
					.options = BT_CONN_LE_OPT_NONE,
					.interval = BT_GAP_SCAN_FAST_INTERVAL,
					.window = BT_GAP_SCAN_FAST_WINDOW,
					.interval_coded = 0,
					.window_coded = 0,
					.timeout = 0,
				};
				static const struct bt_le_conn_param conn_param = {
					.interval_min = BT_GAP_INIT_CONN_INT_MIN,
					.interval_max = BT_GAP_INIT_CONN_INT_MAX,
					.latency = 0,
					.timeout = 400,
				};
				int err = bt_conn_le_create(info->addr,
					&create_param, &conn_param, &relay_conn);
				if (err) {
					printk("relay: connect failed %d\n", err);
					relay_state = RelayState::ERR;
				} else {
					relay_state = RelayState::CONNECTING;
				}
				return;
			}
		}
		net_buf_simple_pull(buf, len);
	}
}

static struct bt_le_scan_cb scan_cb = {
	.recv = scan_recv_cb,
};

/* ── Connection callbacks ──────────────────────────────────────── */

static struct bt_gatt_discover_params disc_params;

static uint8_t discover_cb(struct bt_conn *conn,
                           const struct bt_gatt_attr *attr,
                           struct bt_gatt_discover_params *params)
{
	if (!attr) {
		printk("relay: discovery complete ctrl=%u data=%u stat=%u\n",
		       gatt_ctrl_handle, gatt_data_handle, gatt_stat_handle);
		if (gatt_ctrl_handle && gatt_data_handle && gatt_stat_handle) {
			relay_state = RelayState::READY;
		} else {
			relay_state = RelayState::ERR;
		}
		return BT_GATT_ITER_STOP;
	}

	/* Match characteristic UUIDs to store handles */
	if (bt_uuid_cmp(attr->uuid, BT_UUID_GATT_CHRC) == 0) {
		const auto *chrc = static_cast<const struct bt_gatt_chrc *>(attr->user_data);
		if (bt_uuid_cmp(chrc->uuid, &ota_ctrl_uuid.uuid) == 0) {
			gatt_ctrl_handle = chrc->value_handle;
		} else if (bt_uuid_cmp(chrc->uuid, &ota_data_uuid.uuid) == 0) {
			gatt_data_handle = chrc->value_handle;
		} else if (bt_uuid_cmp(chrc->uuid, &ota_stat_uuid.uuid) == 0) {
			gatt_stat_handle = chrc->value_handle;
		}
	}
	return BT_GATT_ITER_CONTINUE;
}

static void relay_connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("relay: connection failed %d\n", err);
		relay_state = RelayState::ERR;
		return;
	}
	printk("relay: connected, discovering GATT\n");
	relay_state = RelayState::DISCOVERING;

	gatt_ctrl_handle = 0;
	gatt_data_handle = 0;
	gatt_stat_handle = 0;

	disc_params.uuid = &ota_svc_uuid.uuid;
	disc_params.func = discover_cb;
	disc_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
	disc_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
	disc_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

	bt_gatt_discover(conn, &disc_params);
}

static void relay_disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("relay: disconnected reason=%u\n", reason);
	if (relay_conn) {
		bt_conn_unref(relay_conn);
		relay_conn = nullptr;
	}
	if (relay_state == RelayState::READY || relay_state == RelayState::DISCOVERING ||
	    relay_state == RelayState::CONNECTING) {
		relay_state = RelayState::ERR;
	}
}

BT_CONN_CB_DEFINE(relay_conn_cbs) = {
	.connected = relay_connected,
	.disconnected = relay_disconnected,
};

/* ── GATT write callback ──────────────────────────────────────── */

static void gatt_write_cb(struct bt_conn *conn, uint8_t err,
                          struct bt_gatt_write_params *params)
{
	gatt_write_err = err;
	gatt_write_done = true;
}

static struct bt_gatt_write_params write_params;

/* ── GATT read callback ───────────────────────────────────────── */

static uint8_t gatt_read_cb(struct bt_conn *conn, uint8_t err,
                            struct bt_gatt_read_params *params,
                            const void *data, uint16_t length)
{
	if (data && length <= sizeof(gatt_read_buf)) {
		memcpy(gatt_read_buf, data, length);
		gatt_read_len = length;
	}
	gatt_read_done = true;
	return BT_GATT_ITER_STOP;
}

static struct bt_gatt_read_params read_params;

/* ── Relay frame handler ──────────────────────────────────────── */

static void handle_relay_frame(const UartOtaMutableFrame &frame)
{
	using FT = UartOtaProtocol::FrameType;
	auto ft = static_cast<FT>(frame.type);

	if (ft == FT::InfoReq) {
		/* Start OTA session: payload is target name (null-terminated string) */
		if (relay_state != RelayState::IDLE && relay_state != RelayState::ERR) {
			uint8_t rsp = 0xFF;
			send_response(static_cast<uint8_t>(FT::InfoRsp), &rsp, 1);
			return;
		}

		size_t nameLen = frame.payloadLen < sizeof(target_name) - 1
		                 ? frame.payloadLen : sizeof(target_name) - 1;
		memcpy(target_name, frame.payload, nameLen);
		target_name[nameLen] = '\0';

		printk("relay: OTA target=%s\n", target_name);
		relay_state = RelayState::BLE_INIT;

		uint8_t rsp = 0x00;
		send_response(static_cast<uint8_t>(FT::InfoRsp), &rsp, 1);
		return;
	}

	if (relay_state != RelayState::READY) {
		/* Not connected to Tag — reject */
		uint8_t rsp = 0xFF;
		uint8_t rspType = frame.type | 0x01u; /* make it a response type */
		send_response(rspType, &rsp, 1);
		return;
	}

	if (ft == FT::CtrlWrite) {
		/* Forward CTRL write to Tag */
		gatt_write_done = false;
		write_params.handle = gatt_ctrl_handle;
		write_params.offset = 0;
		write_params.data = frame.payload;
		write_params.length = frame.payloadLen;
		write_params.func = gatt_write_cb;
		int err = bt_gatt_write(relay_conn, &write_params);
		if (err) {
			uint8_t rsp = 0xFE;
			send_response(static_cast<uint8_t>(FT::CtrlRsp), &rsp, 1);
			return;
		}
		/* Wait for callback */
		for (int i = 0; i < 300 && !gatt_write_done; ++i) {
			k_sleep(K_MSEC(100));
		}
		uint8_t rsp = gatt_write_done ? static_cast<uint8_t>(gatt_write_err) : 0xFD;
		send_response(static_cast<uint8_t>(FT::CtrlRsp), &rsp, 1);

	} else if (ft == FT::DataWrite) {
		/* Forward DATA write-without-response to Tag — no callback needed */
		int err = bt_gatt_write_without_response(relay_conn, gatt_data_handle,
		                                          frame.payload, frame.payloadLen, false);
		/* Don't send response for data writes (fire-and-forget for throughput) */
		if (err) {
			printk("relay: data write err %d\n", err);
		}

	} else if (ft == FT::StatusReq) {
		/* Read Tag's OTA status */
		gatt_read_done = false;
		gatt_read_len = 0;
		read_params.func = gatt_read_cb;
		read_params.handle_count = 1;
		read_params.single.handle = gatt_stat_handle;
		read_params.single.offset = 0;
		int err = bt_gatt_read(relay_conn, &read_params);
		if (err) {
			uint8_t rsp = 0xFE;
			send_response(static_cast<uint8_t>(FT::StatusRsp), &rsp, 1);
			return;
		}
		for (int i = 0; i < 100 && !gatt_read_done; ++i) {
			k_sleep(K_MSEC(100));
		}
		if (gatt_read_done && gatt_read_len > 0) {
			send_response(static_cast<uint8_t>(FT::StatusRsp),
			              gatt_read_buf, gatt_read_len);
		} else {
			uint8_t rsp = 0xFD;
			send_response(static_cast<uint8_t>(FT::StatusRsp), &rsp, 1);
		}
	}
}

/* ── Public API ────────────────────────────────────────────────── */

int ota_hub_relay_init(const struct device *dev)
{
	host_dev = dev;

	uart_irq_callback_user_data_set(dev, usb_rx_isr, nullptr);
	uart_irq_rx_enable(dev);

	bt_le_scan_cb_register(&scan_cb);

	printk("relay: init OK\n");
	return 0;
}

bool ota_hub_relay_poll(void)
{
	/* Drain USB RX into parser */
	uint8_t byte;
	while (ring_buf_get(&usb_rx_rb, &byte, 1) == 1) {
		UartOtaMutableFrame frame{};
		if (parser.push(byte, frame)) {
			handle_relay_frame(frame);
		}
	}

	/* Handle BLE init / scan state transitions */
	if (relay_state == RelayState::BLE_INIT) {
		int err = bt_enable(nullptr);
		if (err && err != -EALREADY) {
			printk("relay: bt_enable failed %d\n", err);
			relay_state = RelayState::ERR;
		} else {
			printk("relay: scanning for %s\n", target_name);
			static const struct bt_le_scan_param scan_param = {
				.type = BT_LE_SCAN_TYPE_ACTIVE,
				.options = BT_LE_SCAN_OPT_NONE,
				.interval = BT_GAP_SCAN_FAST_INTERVAL,
				.window = BT_GAP_SCAN_FAST_WINDOW,
			};
			err = bt_le_scan_start(&scan_param, nullptr);
			if (err) {
				printk("relay: scan start failed %d\n", err);
				relay_state = RelayState::ERR;
			} else {
				relay_state = RelayState::SCANNING;
			}
		}
	}

	return relay_state != RelayState::IDLE;
}

bool ota_hub_relay_needs_esb_stop(void)
{
	return relay_state == RelayState::BLE_INIT;
}

bool ota_hub_relay_esb_can_restart(void)
{
	if (relay_state == RelayState::DONE || relay_state == RelayState::ERR) {
		if (relay_conn) {
			bt_conn_disconnect(relay_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
			k_sleep(K_MSEC(200));
			if (relay_conn) {
				bt_conn_unref(relay_conn);
				relay_conn = nullptr;
			}
		}
		bt_le_scan_stop();
		bt_disable();
		relay_state = RelayState::IDLE;
		return true;
	}
	return false;
}

#else /* !CONFIG_HELIX_OTA_HUB_RELAY */

int ota_hub_relay_init(const struct device *) { return 0; }
bool ota_hub_relay_poll(void) { return false; }
bool ota_hub_relay_needs_esb_stop(void) { return false; }
bool ota_hub_relay_esb_can_restart(void) { return false; }

#endif /* CONFIG_HELIX_OTA_HUB_RELAY */
