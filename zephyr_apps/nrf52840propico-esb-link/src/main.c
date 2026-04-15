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
	uint32_t anchor_sequence_gaps;
	int32_t offset_min_us;
	int32_t offset_max_us;
	int32_t last_anchor_master_delta_us;
	int32_t last_anchor_local_delta_us;
	int32_t last_anchor_skew_us;
	int32_t anchor_skew_min_us;
	int32_t anchor_skew_max_us;
	uint32_t anchors_suppressed;
	uint32_t anchor_recovery_events;
	uint32_t anchor_missing_count;
	int32_t max_anchor_master_delta_us;
	int32_t max_anchor_local_delta_us;
	uint32_t frames_suppressed;
	uint32_t frame_sequence_gaps;
	uint32_t frame_recovery_events;
	uint32_t frame_missing_count;
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
	HELIX_PACKET_BLACKOUT = 0xB1,
	HELIX_PACKET_FRAME = 0xF1,
};

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});
static const struct device *const telemetry_uart = DEVICE_DT_GET(DT_NODELABEL(uart0));
static struct esb_payload rx_payload;
#if defined(CONFIG_HELIX_ESB_ROLE_NODE)
static struct esb_payload tx_payload;
#endif
static atomic_t tx_ready = ATOMIC_INIT(1);
#if defined(CONFIG_HELIX_ESB_ROLE_MASTER)
static uint8_t next_anchor_sequence;
#endif
#if defined(CONFIG_HELIX_ESB_ROLE_NODE)
static bool have_anchor_sequence;
static uint8_t expected_anchor_sequence;
static bool have_anchor_timestamps;
#endif
#if defined(CONFIG_HELIX_ESB_ROLE_MASTER)
static bool have_frame_sequence;
static uint8_t expected_frame_sequence;
#endif

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

#if defined(CONFIG_HELIX_ESB_ROLE_NODE)
static void status_record_offset(int32_t offset_us)
{
	if (g_helixEsbStatus.anchors_received == 0U) {
		g_helixEsbStatus.offset_min_us = offset_us;
		g_helixEsbStatus.offset_max_us = offset_us;
		return;
	}

	if (offset_us < g_helixEsbStatus.offset_min_us) {
		g_helixEsbStatus.offset_min_us = offset_us;
	}
	if (offset_us > g_helixEsbStatus.offset_max_us) {
		g_helixEsbStatus.offset_max_us = offset_us;
	}
}

static void status_record_anchor_skew(int32_t skew_us)
{
	if (g_helixEsbStatus.anchors_received <= 1U) {
		g_helixEsbStatus.anchor_skew_min_us = skew_us;
		g_helixEsbStatus.anchor_skew_max_us = skew_us;
		return;
	}

	if (skew_us < g_helixEsbStatus.anchor_skew_min_us) {
		g_helixEsbStatus.anchor_skew_min_us = skew_us;
	}
	if (skew_us > g_helixEsbStatus.anchor_skew_max_us) {
		g_helixEsbStatus.anchor_skew_max_us = skew_us;
	}
}

static void status_record_anchor_deltas(int32_t master_delta_us, int32_t local_delta_us)
{
	if (master_delta_us > g_helixEsbStatus.max_anchor_master_delta_us) {
		g_helixEsbStatus.max_anchor_master_delta_us = master_delta_us;
	}
	if (local_delta_us > g_helixEsbStatus.max_anchor_local_delta_us) {
		g_helixEsbStatus.max_anchor_local_delta_us = local_delta_us;
	}
}
#endif

#if defined(CONFIG_HELIX_ESB_ROLE_MASTER)
static bool master_should_suppress_anchor(uint8_t anchor_sequence)
{
	const uint32_t length = CONFIG_HELIX_ESB_MASTER_BLACKOUT_LENGTH;

	if (length == 0U) {
		return false;
	}

	const uint8_t start = (uint8_t)CONFIG_HELIX_ESB_MASTER_BLACKOUT_START_SEQUENCE;
	const uint8_t delta = (uint8_t)(anchor_sequence - start);
	return delta < length;
}
#endif

#if defined(CONFIG_HELIX_ESB_ROLE_NODE)
static bool node_should_suppress_frame(uint8_t frame_sequence)
{
	const uint32_t length = CONFIG_HELIX_ESB_NODE_TX_BLACKOUT_LENGTH;

	if (length == 0U) {
		return false;
	}

	const uint8_t start = (uint8_t)CONFIG_HELIX_ESB_NODE_TX_BLACKOUT_START_SEQUENCE;
	const uint8_t delta = (uint8_t)(frame_sequence - start);
	return delta < length;
}
#endif

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
			if (rx_payload.length >= 3U && rx_payload.data[0] == HELIX_PACKET_FRAME) {
				const uint8_t frame_sequence = rx_payload.data[2];
				if (have_frame_sequence && frame_sequence != expected_frame_sequence) {
					g_helixEsbStatus.frame_sequence_gaps++;
					g_helixEsbStatus.frame_recovery_events++;
					g_helixEsbStatus.frame_missing_count +=
						(uint8_t)(frame_sequence - expected_frame_sequence);
				}
				have_frame_sequence = true;
				expected_frame_sequence = (uint8_t)(frame_sequence + 1U);
			}
			uint32_t master_timestamp_us = now_us();
			uint8_t anchor_sequence = next_anchor_sequence++;
			if (master_should_suppress_anchor(anchor_sequence)) {
				(void)esb_flush_tx();
				struct esb_payload blackout_ack = {
					.pipe = rx_payload.pipe,
					.length = 8,
					.data = {
						HELIX_PACKET_BLACKOUT,
						CONFIG_HELIX_ESB_NODE_ID,
						anchor_sequence,
						(uint8_t)CONFIG_HELIX_ESB_SESSION_TAG,
					},
				};
				memcpy(&blackout_ack.data[4], &master_timestamp_us, sizeof(master_timestamp_us));
				(void)esb_write_payload(&blackout_ack);
				g_helixEsbStatus.anchors_suppressed++;
				continue;
			}
			struct esb_payload ack = {
				.pipe = rx_payload.pipe,
				.length = 8,
				.data = {
					HELIX_PACKET_ANCHOR,
					CONFIG_HELIX_ESB_NODE_ID,
					anchor_sequence,
					(uint8_t)CONFIG_HELIX_ESB_SESSION_TAG,
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
			if (rx_payload.length >= 8U &&
			    rx_payload.data[0] == HELIX_PACKET_ANCHOR &&
			    rx_payload.data[3] == (uint8_t)CONFIG_HELIX_ESB_SESSION_TAG) {
				uint32_t master_timestamp_us = 0;
				uint8_t anchor_sequence = rx_payload.data[2];
				int32_t offset_us;
				uint32_t previous_master_timestamp_us = g_helixEsbStatus.last_master_timestamp_us;
				uint32_t previous_local_timestamp_us = g_helixEsbStatus.last_local_timestamp_us;
				memcpy(&master_timestamp_us, &rx_payload.data[4], sizeof(master_timestamp_us));
				status_store_anchor_bytes(rx_payload.data, rx_payload.length);
				g_helixEsbStatus.ack_payloads++;
				if (have_anchor_sequence && anchor_sequence != expected_anchor_sequence) {
					g_helixEsbStatus.anchor_sequence_gaps++;
					g_helixEsbStatus.anchor_recovery_events++;
					g_helixEsbStatus.anchor_missing_count +=
						(uint8_t)(anchor_sequence - expected_anchor_sequence);
				}
				have_anchor_sequence = true;
				expected_anchor_sequence = (uint8_t)(anchor_sequence + 1U);
				g_helixEsbStatus.anchors_received++;
				g_helixEsbStatus.last_anchor_sequence = anchor_sequence;
				g_helixEsbStatus.last_master_timestamp_us = master_timestamp_us;
				g_helixEsbStatus.last_local_timestamp_us = now_us();
				offset_us =
					(int32_t)(g_helixEsbStatus.last_local_timestamp_us - master_timestamp_us);
				g_helixEsbStatus.estimated_offset_us = offset_us;
				status_record_offset(offset_us);
				if (have_anchor_timestamps) {
					g_helixEsbStatus.last_anchor_master_delta_us =
						(int32_t)(master_timestamp_us - previous_master_timestamp_us);
					g_helixEsbStatus.last_anchor_local_delta_us =
						(int32_t)(g_helixEsbStatus.last_local_timestamp_us -
							  previous_local_timestamp_us);
					status_record_anchor_deltas(
						g_helixEsbStatus.last_anchor_master_delta_us,
						g_helixEsbStatus.last_anchor_local_delta_us);
					g_helixEsbStatus.last_anchor_skew_us =
						g_helixEsbStatus.last_anchor_local_delta_us -
						g_helixEsbStatus.last_anchor_master_delta_us;
					status_record_anchor_skew(g_helixEsbStatus.last_anchor_skew_us);
				}
				have_anchor_timestamps = true;
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

	(void)esb_flush_tx();
	(void)esb_flush_rx();

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

static void telemetry_write(const char *line)
{
	if (!device_is_ready(telemetry_uart)) {
		return;
	}

	for (const char *p = line; *p != '\0'; ++p) {
		uart_poll_out(telemetry_uart, (unsigned char)*p);
	}
}

static void report_status(void)
{
	char line[160];

#if defined(CONFIG_HELIX_ESB_ROLE_MASTER)
	snprintk(line,
		 sizeof(line),
		 "role=master rx=%u ack=%u anchor=%u frame_gaps=%u frame_missing=%u err=%u hb=%u\n",
		 g_helixEsbStatus.rx_packets,
		 g_helixEsbStatus.ack_payloads,
		 g_helixEsbStatus.last_anchor_sequence,
		 g_helixEsbStatus.frame_sequence_gaps,
		 g_helixEsbStatus.frame_missing_count,
		 g_helixEsbStatus.last_error,
		 g_helixEsbStatus.heartbeat);
#else
	snprintk(line,
		 sizeof(line),
		 "role=node tx_ok=%u tx_fail=%u rx=%u anchors=%u anchor_gaps=%u offset=%d skew=%d err=%u hb=%u\n",
		 g_helixEsbStatus.tx_success,
		 g_helixEsbStatus.tx_failed,
		 g_helixEsbStatus.rx_packets,
		 g_helixEsbStatus.anchors_received,
		 g_helixEsbStatus.anchor_sequence_gaps,
		 g_helixEsbStatus.estimated_offset_us,
		 g_helixEsbStatus.last_anchor_skew_us,
		 g_helixEsbStatus.last_error,
		 g_helixEsbStatus.heartbeat);
#endif

	telemetry_write(line);
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
	uint8_t frame_sequence = (uint8_t)(g_helixEsbStatus.tx_attempts & 0xFF);
	if (node_should_suppress_frame(frame_sequence)) {
		g_helixEsbStatus.frames_suppressed++;
		g_helixEsbStatus.tx_attempts++;
		atomic_set(&tx_ready, 1);
		return;
	}
	tx_payload.data[2] = frame_sequence;
	tx_payload.data[3] = (uint8_t)(g_helixEsbStatus.heartbeat & 0xFF);
	uint32_t tx_local_timestamp_us = now_us();
	int32_t estimated_offset_us = g_helixEsbStatus.estimated_offset_us;
	memcpy(&tx_payload.data[4], &tx_local_timestamp_us, sizeof(tx_local_timestamp_us));
	memcpy(&tx_payload.data[8], &estimated_offset_us, sizeof(estimated_offset_us));

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
	g_helixEsbStatus.offset_min_us = INT32_MAX;
	g_helixEsbStatus.offset_max_us = INT32_MIN;
	g_helixEsbStatus.anchor_skew_min_us = INT32_MAX;
	g_helixEsbStatus.anchor_skew_max_us = INT32_MIN;
	g_helixEsbStatus.max_anchor_master_delta_us = 0;
	g_helixEsbStatus.max_anchor_local_delta_us = 0;
	telemetry_write("helix-esb boot\n");

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
		if ((g_helixEsbStatus.heartbeat % 10U) == 0U) {
			report_status();
		}
		k_msleep(CONFIG_HELIX_ESB_SEND_PERIOD_MS);
	}
}
