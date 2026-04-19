#include "ota_hub_relay.hpp"

#if defined(CONFIG_HELIX_OTA_HUB_RELAY)

#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <string.h>

extern "C" {
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
}

#include "UartOtaProtocol.hpp"

/* Declared in main.cpp — requests the Hub to inject an ESB OTA trigger
 * into ACK payloads for the target node. retries=0 means one-shot. */
extern "C" void helix_request_ota_trigger(uint8_t node_id, uint8_t retries);

using helix::UartOtaProtocol;
using helix::UartOtaMutableFrame;
using helix::UartOtaFrameParser;

/* Binary semaphore for DATA_WRITE flow control.  bt_gatt_write_without_response()
 * returns 0 as soon as the write is queued locally, not when the PDU has been
 * ACKed on-air.  Without flow control the Hub queues faster than the Tag's
 * ACL RX buffers can absorb — PDUs get dropped silently (write-without-response
 * has no NACK).  The *_cb variant invokes our callback when the controller
 * reports the PDU was sent, giving us real per-chunk flow control. */
static K_SEM_DEFINE(data_tx_sem, 1, 1);

static void data_tx_complete_cb(struct bt_conn *conn, void *user_data)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(user_data);
	k_sem_give(&data_tx_sem);
}

/* ── USB CDC RX ────────────────────────────────────────────────── */
/* The new USB device stack (CONFIG_USB_DEVICE_STACK_NEXT) only primes
 * the bulk OUT endpoint when uart_irq_rx_enable() is called. Pure
 * uart_poll_in() never triggers endpoint queueing, so the host write
 * hangs indefinitely. We call uart_irq_rx_enable() at init to prime
 * the endpoint, then use uart_poll_in() to read from the internal
 * ring buffer. The endpoint re-primes on each read. */

static const struct device *host_dev;
static UartOtaFrameParser<512> parser;

/* IRQ-driven RX: the CDC ACM callback runs in the system workqueue
 * context and drains data from the CDC ACM internal FIFO into our
 * ring buffer. The main loop reads from our ring buffer. */
#define RX_RING_SIZE 512
static uint8_t rx_ring_buf[RX_RING_SIZE];
static volatile uint16_t rx_head; /* callback writes */
static volatile uint16_t rx_tail; /* main loop reads */

static void uart_rx_callback(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
		if (uart_irq_rx_ready(dev)) {
			uint8_t buf[64];
			int len = uart_fifo_read(dev, buf, sizeof(buf));
			for (int i = 0; i < len; ++i) {
				uint16_t next = (rx_head + 1) % RX_RING_SIZE;
				if (next != rx_tail) {
					rx_ring_buf[rx_head] = buf[i];
					rx_head = next;
				}
			}
		}
	}
}

static bool rx_ring_get(uint8_t *byte)
{
	if (rx_tail == rx_head) {
		return false;
	}
	*byte = rx_ring_buf[rx_tail];
	rx_tail = (rx_tail + 1) % RX_RING_SIZE;
	return true;
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

static void scan_result_cb(const bt_addr_le_t *addr, int8_t rssi,
                           uint8_t adv_type, struct net_buf_simple *buf)
{
	ARG_UNUSED(rssi);
	ARG_UNUSED(adv_type);

	/* Parse all AD structures in one pass, capturing name and
	 * manufacturer data. */
	char found_name[16] = {};
	uint8_t mfg_buf[24] = {};
	uint8_t mfg_len = 0;

	while (buf->len > 1) {
		uint8_t len = net_buf_simple_pull_u8(buf);
		if (len == 0 || len > buf->len) {
			break;
		}
		uint8_t type = net_buf_simple_pull_u8(buf);
		len--;
		if ((type == BT_DATA_NAME_COMPLETE || type == BT_DATA_NAME_SHORTENED) &&
		    len > 0 && len < sizeof(found_name)) {
			memcpy(found_name, buf->data, len);
			found_name[len] = '\0';
		} else if (type == BT_DATA_MANUFACTURER_DATA &&
		           len > 0 && len <= sizeof(mfg_buf)) {
			memcpy(mfg_buf, buf->data, len);
			mfg_len = len;
		}
		net_buf_simple_pull(buf, len);
	}

	if (found_name[0] == '\0' ||
	    strncmp(found_name, target_name, strlen(target_name)) != 0) {
		return;
	}

	printk("relay: found %s (mfg %u bytes:", found_name, mfg_len);
	for (uint8_t i = 0; i < mfg_len && i < 16; i++) {
		printk(" %02x", mfg_buf[i]);
	}
	printk(")\n");

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
		.timeout = 3200, /* 32s — Tag flash erase takes ~10s */
	};
	int err = bt_conn_le_create(addr, &create_param, &conn_param, &relay_conn);
	if (err) {
		printk("relay: connect failed %d\n", err);
		relay_state = RelayState::ERR;
	} else {
		relay_state = RelayState::CONNECTING;
	}
}

/* ── Connection callbacks ──────────────────────────────────────── */

static struct bt_gatt_discover_params disc_params;

/* Phase 2: discover characteristics within the service handle range */
static uint8_t discover_chrc_cb(struct bt_conn *conn,
                                const struct bt_gatt_attr *attr,
                                struct bt_gatt_discover_params *params)
{
	if (!attr) {
		printk("relay: discovery done ctrl=%u data=%u stat=%u\n",
		       gatt_ctrl_handle, gatt_data_handle, gatt_stat_handle);
		if (gatt_ctrl_handle && gatt_data_handle && gatt_stat_handle) {
			relay_state = RelayState::READY;
		} else {
			relay_state = RelayState::ERR;
		}
		return BT_GATT_ITER_STOP;
	}

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

/* Phase 1: discover the OTA primary service to get handle range */
static uint8_t discover_svc_cb(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr,
                               struct bt_gatt_discover_params *params)
{
	if (!attr) {
		printk("relay: OTA service not found\n");
		relay_state = RelayState::ERR;
		return BT_GATT_ITER_STOP;
	}

	const auto *svc = static_cast<const struct bt_gatt_service_val *>(attr->user_data);
	printk("relay: OTA svc %u-%u\n", attr->handle, svc->end_handle);

	/* Phase 2: discover characteristics within this service */
	disc_params.uuid = nullptr;
	disc_params.func = discover_chrc_cb;
	disc_params.start_handle = attr->handle + 1;
	disc_params.end_handle = svc->end_handle;
	disc_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
	bt_gatt_discover(conn, &disc_params);

	return BT_GATT_ITER_STOP;
}

static void relay_connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("relay: connection failed %d\n", err);
		relay_state = RelayState::ERR;
		return;
	}
	printk("relay: connected\n");
	relay_state = RelayState::DISCOVERING;

	/* Request longer supervision timeout for flash erase (~10s) */
	{
		static const struct bt_le_conn_param update_param = {
			.interval_min = 6,   /* 7.5ms */
			.interval_max = 24,  /* 30ms */
			.latency = 0,
			.timeout = 3200,     /* 32s */
		};
		bt_conn_le_param_update(conn, &update_param);
	}

	gatt_ctrl_handle = 0;
	gatt_data_handle = 0;
	gatt_stat_handle = 0;

	/* Phase 1: find the OTA primary service */
	disc_params.uuid = &ota_svc_uuid.uuid;
	disc_params.func = discover_svc_cb;
	disc_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
	disc_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
	disc_params.type = BT_GATT_DISCOVER_PRIMARY;

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

	if (ft == FT::EsbTrigReq) {
		/* Request Hub to inject an ESB OTA trigger for a node.
		 * Payload: [node_id, retries].  ESB is still running at this point. */
		if (frame.payloadLen >= 2) {
			helix_request_ota_trigger(frame.payload[0], frame.payload[1]);
			printk("relay: esb trigger node=%u retries=%u\n",
			       frame.payload[0], frame.payload[1]);
			uint8_t rsp = 0x00;
			send_response(static_cast<uint8_t>(FT::EsbTrigRsp), &rsp, 1);
		} else {
			uint8_t rsp = 0xFF;
			send_response(static_cast<uint8_t>(FT::EsbTrigRsp), &rsp, 1);
		}
		return;
	}

	if (ft == FT::InfoReq) {
		/* Start OTA session: payload is target name (null-terminated string).
		 * Don't send InfoRsp yet — ESB is still running and binary response
		 * bytes would be interleaved with ASCII FRAME data. The response is
		 * deferred until after ESB is stopped (in the BLE_INIT handler). */
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
		/* Forward CTRL write to Tag. Copy payload to a stable buffer
		 * since bt_gatt_write is async and the parser buffer may be
		 * reused during the k_sleep wait. */
		gatt_write_done = false;
		static uint8_t ctrl_buf[64];
		size_t copyLen = frame.payloadLen < sizeof(ctrl_buf)
		                 ? frame.payloadLen : sizeof(ctrl_buf);
		memcpy(ctrl_buf, frame.payload, copyLen);
		write_params.handle = gatt_ctrl_handle;
		write_params.offset = 0;
		write_params.data = ctrl_buf;
		write_params.length = copyLen;
		write_params.func = gatt_write_cb;
		int err = bt_gatt_write(relay_conn, &write_params);
		if (err) {
			uint8_t rsp = 0xFE;
			send_response(static_cast<uint8_t>(FT::CtrlRsp), &rsp, 1);
			return;
		}
		/* Wait for callback — BEGIN triggers flash erase (~10s) */
		for (int i = 0; i < 600 && !gatt_write_done; ++i) {
			k_sleep(K_MSEC(100));
		}
		uint8_t rsp = gatt_write_done ? static_cast<uint8_t>(gatt_write_err) : 0xFD;
		send_response(static_cast<uint8_t>(FT::CtrlRsp), &rsp, 1);

	} else if (ft == FT::DataWrite) {
		/* Use write-WITH-response so Hub waits for Tag's ATT Write Response
		 * per chunk. This gives true flow control — the Tag has processed
		 * the chunk (handleDataWrite returned) before we send the next.
		 * Write-without-response (even with the _cb variant) only waits
		 * for local TX queue, not remote-side processing, so we were
		 * overrunning the Tag's ACL RX buffer + flash-write throughput. */
		static uint8_t dw_buf[256];
		size_t dw_len = frame.payloadLen < sizeof(dw_buf)
		                 ? frame.payloadLen : sizeof(dw_buf);
		memcpy(dw_buf, frame.payload, dw_len);
		gatt_write_done = false;
		write_params.handle = gatt_data_handle;
		write_params.offset = 0;
		write_params.data = dw_buf;
		write_params.length = dw_len;
		write_params.func = gatt_write_cb;
		int err = -ENOMEM;
		for (int retries = 0; err == -ENOMEM && retries < 50; retries++) {
			err = bt_gatt_write(relay_conn, &write_params);
			if (err == -ENOMEM) {
				k_sleep(K_MSEC(5));
			}
		}
		uint8_t dw_rsp;
		if (err == 0) {
			/* Wait up to 2s for Tag's ATT Write Response */
			for (int i = 0; i < 200 && !gatt_write_done; ++i) {
				k_sleep(K_MSEC(10));
			}
			dw_rsp = gatt_write_done ? static_cast<uint8_t>(gatt_write_err) : 0xFD;
		} else {
			dw_rsp = 0xFE;
		}
		/* Send DataRsp so uploader waits for per-chunk ACK — this is the
		 * back-pressure mechanism that prevents the Hub's USB CDC RX ring
		 * buffer from overrunning (uploader sends chunks far faster than
		 * the Hub can relay them over BLE). */
		send_response(static_cast<uint8_t>(FT::DataRsp), &dw_rsp, 1);

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

	/* Set up IRQ-driven RX. The CDC ACM driver in the new USB stack
	 * only primes the bulk OUT endpoint when uart_irq_rx_enable() is
	 * called. The callback drains the CDC ACM FIFO into our ring buffer
	 * using the proper uart_fifo_read API. */
	uart_irq_callback_user_data_set(dev, uart_rx_callback, nullptr);
	uart_irq_rx_enable(dev);
	printk("relay: init OK (irq rx)\n");
	return 0;
}

static uint32_t rx_byte_count;
static uint32_t poll_count;

bool ota_hub_relay_poll(void)
{
	poll_count++;

	/* Drain IRQ ring buffer and feed into frame parser */
	uint8_t byte;
	while (rx_ring_get(&byte)) {
		rx_byte_count++;
		UartOtaMutableFrame frame{};
		if (parser.push(byte, frame)) {
			handle_relay_frame(frame);
		}
	}

	/* Handle BLE init / scan state transitions.
	 * When BLE_INIT is first set (by handle_relay_frame), return
	 * immediately so the main loop can disable ESB before we call
	 * bt_enable(). The radio is shared — ESB must be off first. */
	if (relay_state == RelayState::BLE_INIT) {
		static bool esb_stop_requested;
		if (!esb_stop_requested) {
			esb_stop_requested = true;
			return true; /* signal main loop to stop ESB */
		}
		esb_stop_requested = false;

		/* Send deferred InfoRsp now that ESB is stopped and CDC TX is clean */
		{
			using FT = UartOtaProtocol::FrameType;
			uint8_t rsp = 0x00;
			send_response(static_cast<uint8_t>(FT::InfoRsp), &rsp, 1);
		}

		/* Let LF clock / MPSL calibrate before bt_enable() — otherwise
		 * the first connection after a fast reset can drop with
		 * supervision-timeout (reason=8) within ~3 s. See equivalent
		 * note in Tag's run_ota_boot_window(). */
		k_sleep(K_MSEC(2000));

		int err = bt_enable(nullptr);
		if (err && err != -EALREADY) {
			relay_state = RelayState::ERR;
		} else {
			static const struct bt_le_scan_param scan_param = {
				.type = BT_LE_SCAN_TYPE_PASSIVE,
				.options = BT_LE_SCAN_OPT_NONE,
				.interval = BT_GAP_SCAN_FAST_INTERVAL,
				.window = BT_GAP_SCAN_FAST_WINDOW,
			};
			err = bt_le_scan_start(&scan_param, scan_result_cb);
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
	/* ESB must be off whenever BLE is needed */
	return relay_state == RelayState::BLE_INIT ||
	       relay_state == RelayState::SCANNING ||
	       relay_state == RelayState::CONNECTING ||
	       relay_state == RelayState::DISCOVERING ||
	       relay_state == RelayState::READY;
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

uint32_t ota_hub_relay_rx_count(void) { return rx_byte_count; }
uint32_t ota_hub_relay_poll_count(void) { return poll_count; }

#else /* !CONFIG_HELIX_OTA_HUB_RELAY */

int ota_hub_relay_init(const struct device *) { return 0; }
bool ota_hub_relay_poll(void) { return false; }
bool ota_hub_relay_needs_esb_stop(void) { return false; }
bool ota_hub_relay_esb_can_restart(void) { return false; }
uint32_t ota_hub_relay_rx_count(void) { return 0; }
uint32_t ota_hub_relay_poll_count(void) { return 0; }

#endif /* CONFIG_HELIX_OTA_HUB_RELAY */
