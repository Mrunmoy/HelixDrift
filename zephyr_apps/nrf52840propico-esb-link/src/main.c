#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>
#include <zephyr/types.h>
#include <zephyr/sys_clock.h>

#include <esb.h>
#include <nrfx.h>

LOG_MODULE_REGISTER(helix_esb_link, LOG_LEVEL_INF);

struct HelixEsbStatus {
	uint32_t magic;
	uint32_t role;
	uint32_t phase;
	uint32_t last_event;
	uint32_t tx_attempts;
	uint32_t tx_success;
	uint32_t tx_failed;
	uint32_t rx_packets;
	uint32_t ack_payloads;
	uint32_t last_seq;
	uint32_t last_rx_node;
	uint32_t last_rx_len;
	uint32_t last_error;
	uint32_t heartbeat;
	uint32_t anchors_received;
	uint32_t anchors_sent;
	uint32_t last_anchor_sequence;
	int32_t estimated_offset_us;
	uint32_t last_master_timestamp_us;
	uint32_t last_local_timestamp_us;
	uint32_t last_anchor_raw_word0;
	uint32_t last_anchor_raw_word1;
};

volatile struct HelixEsbStatus g_helixEsbStatus;

enum {
	HELIX_ROLE_MASTER = 1,
	HELIX_ROLE_NODE = 2,
};

enum {
	HELIX_PHASE_BOOT = 1,
	HELIX_PHASE_CLOCK_READY = 2,
	HELIX_PHASE_ESB_READY = 3,
	HELIX_PHASE_RUNNING = 4,
};

enum {
	HELIX_EVENT_NONE = 0,
	HELIX_EVENT_TX_SUCCESS = 1,
	HELIX_EVENT_TX_FAILED = 2,
	HELIX_EVENT_RX = 3,
};

enum {
	HELIX_PACKET_ANCHOR = 0xA1,
	HELIX_PACKET_FRAME = 0xF1,
};

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});
static struct esb_payload rx_payload;
#if defined(CONFIG_HELIX_ESB_ROLE_NODE)
static struct esb_payload tx_payload;
#endif
static atomic_t tx_ready = ATOMIC_INIT(1);
static uint8_t next_anchor_sequence;

static void status_store_anchor_bytes(const uint8_t *data, size_t len)
{
	uint32_t word0 = 0;
	uint32_t word1 = 0;

	if (len >= 4U) {
		memcpy(&word0, &data[0], sizeof(word0));
	}
	if (len >= 8U) {
		memcpy(&word1, &data[4], sizeof(word1));
	}

	g_helixEsbStatus.last_anchor_raw_word0 = word0;
	g_helixEsbStatus.last_anchor_raw_word1 = word1;
}

static uint32_t now_us(void)
{
	return (uint32_t)(k_uptime_get() * 1000LL);
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

static void status_set_error(int err)
{
	g_helixEsbStatus.last_error = (uint32_t)(err < 0 ? -err : err);
}

static void event_handler(struct esb_evt const *event)
{
	switch (event->evt_id) {
	case ESB_EVENT_TX_SUCCESS:
		g_helixEsbStatus.last_event = HELIX_EVENT_TX_SUCCESS;
		g_helixEsbStatus.tx_success++;
		atomic_set(&tx_ready, 1);
		break;
	case ESB_EVENT_TX_FAILED:
		g_helixEsbStatus.last_event = HELIX_EVENT_TX_FAILED;
		g_helixEsbStatus.tx_failed++;
		(void)esb_flush_tx();
		atomic_set(&tx_ready, 1);
		break;
	case ESB_EVENT_RX_RECEIVED:
		g_helixEsbStatus.last_event = HELIX_EVENT_RX;
		while (esb_read_rx_payload(&rx_payload) == 0) {
			g_helixEsbStatus.rx_packets++;
			g_helixEsbStatus.last_rx_len = rx_payload.length;
			if (rx_payload.length >= 3U) {
				g_helixEsbStatus.last_rx_node = rx_payload.data[1];
				g_helixEsbStatus.last_seq = rx_payload.data[2];
			}
#if defined(CONFIG_HELIX_ESB_ROLE_MASTER)
			uint32_t master_timestamp_us = now_us();
			struct esb_payload ack = {
				.pipe = rx_payload.pipe,
				.length = 8,
				.data = {
					HELIX_PACKET_ANCHOR,
					CONFIG_HELIX_ESB_NODE_ID,
					next_anchor_sequence++,
					0,
				},
			};
			memcpy(&ack.data[4], &master_timestamp_us, sizeof(master_timestamp_us));
			(void)esb_write_payload(&ack);
			status_store_anchor_bytes(ack.data, ack.length);
			g_helixEsbStatus.ack_payloads++;
			g_helixEsbStatus.anchors_sent++;
			g_helixEsbStatus.last_anchor_sequence = ack.data[2];
			g_helixEsbStatus.last_master_timestamp_us = master_timestamp_us;
#else
			if (rx_payload.length >= 8U && rx_payload.data[0] == HELIX_PACKET_ANCHOR) {
				uint32_t master_timestamp_us = 0;
				memcpy(&master_timestamp_us, &rx_payload.data[4], sizeof(master_timestamp_us));
				status_store_anchor_bytes(rx_payload.data, rx_payload.length);
				g_helixEsbStatus.ack_payloads++;
				g_helixEsbStatus.anchors_received++;
				g_helixEsbStatus.last_anchor_sequence = rx_payload.data[2];
				g_helixEsbStatus.last_master_timestamp_us = master_timestamp_us;
				g_helixEsbStatus.last_local_timestamp_us = now_us();
				g_helixEsbStatus.estimated_offset_us =
					(int32_t)(g_helixEsbStatus.last_local_timestamp_us - master_timestamp_us);
			}
#endif
		}
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
#if defined(CONFIG_HELIX_ESB_ROLE_MASTER)
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

	err = esb_set_rf_channel(CONFIG_HELIX_ESB_RF_CHANNEL);
	if (err) {
		return err;
	}

	return 0;
}

static void led_set(bool on)
{
	if (gpio_is_ready_dt(&led)) {
		(void)gpio_pin_set_dt(&led, on ? 1 : 0);
	}
}

static void heartbeat_tick(void)
{
	static bool on;
	on = !on;
	led_set(on);
	g_helixEsbStatus.heartbeat++;
}

static void maybe_send_packet(void)
{
#if defined(CONFIG_HELIX_ESB_ROLE_NODE)
	if (!atomic_cas(&tx_ready, 1, 0)) {
		return;
	}

	tx_payload.pipe = CONFIG_HELIX_ESB_PIPE;
	tx_payload.noack = 0;
	tx_payload.length = 12;
	tx_payload.data[0] = HELIX_PACKET_FRAME;
	tx_payload.data[1] = (uint8_t)CONFIG_HELIX_ESB_NODE_ID;
	tx_payload.data[2] = (uint8_t)(g_helixEsbStatus.tx_attempts & 0xFF);
	tx_payload.data[3] = (uint8_t)(g_helixEsbStatus.heartbeat & 0xFF);
	g_helixEsbStatus.last_local_timestamp_us = now_us();
	memcpy(&tx_payload.data[4], &g_helixEsbStatus.last_local_timestamp_us,
	       sizeof(g_helixEsbStatus.last_local_timestamp_us));
	memcpy(&tx_payload.data[8], &g_helixEsbStatus.estimated_offset_us,
	       sizeof(g_helixEsbStatus.estimated_offset_us));

	g_helixEsbStatus.tx_attempts++;
	int err = esb_write_payload(&tx_payload);
	if (err) {
		status_set_error(err);
		atomic_set(&tx_ready, 1);
	}
#endif
}

int main(void)
{
	int err;

	g_helixEsbStatus.magic = 0x48455342U;
	g_helixEsbStatus.role =
#if defined(CONFIG_HELIX_ESB_ROLE_MASTER)
		HELIX_ROLE_MASTER;
#else
		HELIX_ROLE_NODE;
#endif
	g_helixEsbStatus.phase = HELIX_PHASE_BOOT;

	if (gpio_is_ready_dt(&led)) {
		err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
		if (err) {
			status_set_error(err);
			return 1;
		}
	}

	err = clocks_start();
	if (err) {
		status_set_error(err);
		return 2;
	}
	g_helixEsbStatus.phase = HELIX_PHASE_CLOCK_READY;

	err = esb_initialize();
	if (err) {
		status_set_error(err);
		return 3;
	}
	g_helixEsbStatus.phase = HELIX_PHASE_ESB_READY;

	printk("helix esb link boot role=%s node=%d ch=%d\n",
#if defined(CONFIG_HELIX_ESB_ROLE_MASTER)
	       "master",
#else
	       "node",
#endif
	       CONFIG_HELIX_ESB_NODE_ID, CONFIG_HELIX_ESB_RF_CHANNEL);

	g_helixEsbStatus.phase = HELIX_PHASE_RUNNING;

#if defined(CONFIG_HELIX_ESB_ROLE_MASTER)
	err = esb_start_rx();
	if (err) {
		status_set_error(err);
		return 4;
	}
#endif

	while (true) {
		heartbeat_tick();
		maybe_send_packet();
		k_msleep(CONFIG_HELIX_ESB_SEND_PERIOD_MS);
	}
}
