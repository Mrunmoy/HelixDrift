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
#include <string.h>

#include <esb.h>

#include "usbd_init.h"

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
