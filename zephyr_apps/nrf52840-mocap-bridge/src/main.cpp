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
#include "ota_hub_relay.hpp"

#if defined(CONFIG_BT)
#include <zephyr/bluetooth/bluetooth.h>
#endif

#if defined(CONFIG_BT) && defined(CONFIG_HELIX_MOCAP_BRIDGE_ROLE_NODE)
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/devicetree/fixed-partitions.h>
#include <zephyr/dfu/mcuboot.h>
#include <pm_config.h>
#include <zephyr/storage/flash_map.h>

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
	HELIX_PACKET_OTA_TRIGGER = 0xE1,
};

/* Magic value to prevent accidental OTA triggers. Must match on both sides. */
static constexpr uint32_t kOtaTriggerMagic = 0xDEADBEEFu;

struct __packed HelixSyncAnchor {
	uint8_t type;
	uint8_t central_id;
	uint8_t anchor_sequence;
	uint8_t session_tag;
	uint32_t central_timestamp_us;
	/* v2+: flags byte. bit0 = tell target tag to reboot into BLE OTA
	 * window. Anchors of the OLD 8-byte size are still accepted
	 * (flags defaults to 0 when absent). */
	uint8_t flags;
	/* v3+: target node_id for OTA trigger. Needed because Hub now
	 * FLOODS the OTA_REQ flag in every anchor during the trigger window
	 * (not just the target's ACKs) — so each receiving Tag must be able
	 * to tell whether the flag is for it. 0xFF = broadcast (all nodes).
	 * Tags that see the old 9-byte anchor (no target_id field) treat
	 * the OTA_REQ flag as broadcast — preserves legacy single-target
	 * behaviour for fleets that haven't migrated to v3 anchors yet. */
	uint8_t ota_target_node_id;
};

static constexpr uint8_t HELIX_ANCHOR_FLAG_OTA_REQ = 0x01u;

struct __packed HelixOtaTrigger {
	uint8_t type;           /* HELIX_PACKET_OTA_TRIGGER */
	uint8_t central_id;
	uint8_t target_node_id; /* 0xFF = broadcast */
	uint8_t session_tag;
	uint32_t magic;         /* kOtaTriggerMagic */
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

/* ── Persistent node_id (flash-backed) ──────────────────────────────
 *
 * Single firmware binary for all 10 Tags. Each Tag's unique node_id is
 * stored in the first word of the settings_storage partition at 0xFE000
 * (we have CONFIG_SETTINGS=n so that 8 KB page is otherwise unused).
 *
 * Layout: [node_id:u8][0xFF:u8][0xFF:u8][0xFF:u8]
 *   lower byte = node_id (1..255); sentinel 0xFFFFFFFF = unprovisioned.
 *
 * Provisioning (SWD): after `nrfutil device program` (which erases the
 * whole flash, so this word ends up as 0xFFFFFFFF), write the node_id:
 *   nrfutil device write --address 0xFE000 --value 0xFFFFFF<nn> --direct
 *
 * If unprovisioned, we fall back to CONFIG_HELIX_MOCAP_NODE_ID so dev
 * workflow still works on a single Tag.
 *
 * OTA never touches this page (MCUboot overwrite-only only writes
 * mcuboot_secondary at 0x85000-0xFE000), so node_id persists forever
 * after provisioning.
 */
static constexpr uint32_t kHelixConfigAddr = 0xFE000u;
#if defined(PM_SETTINGS_STORAGE_ADDRESS)
static_assert(kHelixConfigAddr == PM_SETTINGS_STORAGE_ADDRESS,
              "node_id flash address must match settings_storage partition base");
#endif
static uint8_t g_node_id = (uint8_t)CONFIG_HELIX_MOCAP_NODE_ID;

static uint8_t helix_load_node_id(void)
{
	const uint32_t val = *reinterpret_cast<const volatile uint32_t *>(kHelixConfigAddr);
	if (val == 0xFFFFFFFFu) {
		return (uint8_t)CONFIG_HELIX_MOCAP_NODE_ID;
	}
	const uint8_t id = (uint8_t)(val & 0xFFu);
	if (id == 0u) {
		return (uint8_t)CONFIG_HELIX_MOCAP_NODE_ID;
	}
	return id;
}

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});
static const struct device *const host_stream = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
static struct esb_payload rx_payload;
#if defined(CONFIG_HELIX_MOCAP_BRIDGE_ROLE_NODE)
static struct esb_payload tx_payload;
static atomic_t tx_ready = ATOMIC_INIT(1);
static bool have_anchor;
static int32_t estimated_offset_us;
/* Set from ESB ISR when an OTA trigger arrives; main loop reboots. */
static atomic_t ota_reboot_pending = ATOMIC_INIT(0);

/* ── RF-sync instrumentation (Stage 1 per docs/RF_SYNC_DECISION_LOG.md) ──
 * Tag side. Histograms stay in RAM and get emitted in the SUMMARY line.
 * last_tx_local_us: local_us at the moment we queued the most recent
 *   ESB TX frame. Pair it against anchor RX time to see how quickly
 *   Hub's anchor came back.
 * anchor_age_bucket: distribution of (local_at_anchor_rx - last_tx_local_us)
 *   buckets [<2 ms, 2-10 ms, 10-30 ms, >=30 ms]. A bimodal distribution
 *   with most weight in bucket 0 means anchors mostly hit Hub's TIFS
 *   window; weight in buckets 2/3 means the slow-path is dominant.
 * offset_step_bucket: distribution of |new_offset - old_offset| across
 *   anchors — large jumps mean bad / stale anchors are corrupting the
 *   estimator (a key symptom of the shared-pipe cross-contamination). */
static volatile uint32_t last_tx_local_us;
static volatile uint32_t anchor_age_bucket[4];
static volatile uint32_t offset_step_bucket[4];
#endif
#if defined(CONFIG_HELIX_MOCAP_BRIDGE_ROLE_CENTRAL)
static struct NodeTrack tracked_nodes[CONFIG_HELIX_MOCAP_MAX_TRACKED_NODES];
static uint8_t next_anchor_sequence;
/* When non-zero, the central injects an OTA trigger into the next ACK
 * payload for the target node (0xFF = broadcast). Cleared after send. */
static volatile uint8_t ota_trigger_target_node;
static volatile uint8_t ota_trigger_retries;
static volatile uint32_t ota_triggers_sent;

/* ── RF-sync instrumentation (Stage 1 per docs/RF_SYNC_DECISION_LOG.md) ──
 * Hub side. Measures whether Hub's anchor write makes the ~150 µs TIFS
 * window (fast path → anchor rides current frame's ACK, ~µs delay) or
 * misses it (slow path → anchor stays queued in pipe-0 FIFO and rides
 * a later frame's ACK, ~ms delay). Core measurement that turns the
 * "~70% slow path" hypothesis into a number.
 *
 * A naive single-slot last_queue_us has an asymmetric bias: when
 * several RX events fire before the oldest queued anchor's TX_SUCCESS
 * lands, the single slot gets overwritten and the oldest-anchor's
 * latency is measured against the newest queue time — either dropped
 * (negative delta) or compressed toward small buckets. That hides the
 * exact "old anchor waited behind a burst of RX" pattern we care
 * about. Fixed here with a FIFO ring matched against the ESB
 * ACK-payload TX FIFO. Both head and tail are only touched from the
 * ESB event-handler context (single-ISR execution in NCS v3.2.4), so
 * no locking is needed.
 *
 * Ring size: ESB_TX_FIFO_SIZE defaults to 8 in NCS; 16 leaves
 * headroom for any burst. Must be a power of 2 for the mask trick.
 * anchor_pending_max: observed peak FIFO depth (head - tail). */
#define HELIX_ANCHOR_QUEUE_RING   16u
#define HELIX_ANCHOR_QUEUE_MASK   (HELIX_ANCHOR_QUEUE_RING - 1u)
static_assert((HELIX_ANCHOR_QUEUE_RING & HELIX_ANCHOR_QUEUE_MASK) == 0u,
              "HELIX_ANCHOR_QUEUE_RING must be a power of 2");

static volatile uint32_t anchor_queue_ts_ring[HELIX_ANCHOR_QUEUE_RING];
static volatile uint32_t anchor_queue_ring_head;   /* next slot to write */
static volatile uint32_t anchor_queue_ring_tail;   /* next slot to consume */
static volatile uint32_t anchor_pending_max;
static volatile uint32_t ack_tx_latency_bucket[4];
static volatile uint32_t ack_tx_dropped_no_queue;  /* TX_SUCCESS with ring empty — diagnostic */

/* Request an OTA trigger for a specific node (or 0xFF = broadcast).
 * Sends the trigger on the next few FRAMEs from the target, then clears.
 * Called from the Hub relay when the PC requests a remote OTA trigger.
 *
 * Tear-down first, then arm. The ESB RX ISR reads ota_trigger_target_node
 * and ota_trigger_retries as an implicit pair; if we raced the two volatile
 * writes in the wrong order, the ISR could read the new target against the
 * old retries (or vice versa) and either send a stale flood or skip a new
 * one. Gate on target_node=0, barrier, then arm retries + target. The DMBs
 * force ordering on Cortex-M4 — cheap and sufficient since the ISR is on
 * the same core. (Copilot code review, 2026-04-20.) */
extern "C" void helix_request_ota_trigger(uint8_t node_id, uint8_t retries)
{
	ota_trigger_target_node = 0u;  /* gate: ISR ignores trigger during update */
	__DMB();
	ota_trigger_retries = retries;
	__DMB();
	ota_trigger_target_node = node_id;  /* arm */
}
#endif

static uint32_t now_us(void)
{
	return (uint32_t)(k_uptime_get() * 1000LL);
}

/* Bucket a µs-delta into 4 bins: [<2 ms, 2-10 ms, 10-30 ms, >=30 ms].
 * Used by the RF-sync instrumentation. Bumps bucket_arr[idx] and returns
 * the bucket index (0-3). Non-atomic — we accept occasional lost counts
 * from RX-ISR vs. main-thread races; instrumentation, not invariants. */
static inline uint32_t histo_bucket_us(uint32_t delta_us, volatile uint32_t *bucket_arr)
{
	uint32_t idx;
	if (delta_us < 2000u) idx = 0u;
	else if (delta_us < 10000u) idx = 1u;
	else if (delta_us < 30000u) idx = 2u;
	else idx = 3u;
	bucket_arr[idx]++;
	return idx;
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
	/* SUMMARY line can reach ~260 chars on node role with the Stage-1
	 * instrumentation buckets appended. Bump buffer accordingly. */
	char line[320];

#if defined(CONFIG_HELIX_MOCAP_BRIDGE_ROLE_CENTRAL)
	/* ack_lat: per-anchor FIFO-ring latency histogram. Buckets:
	 *   <2 ms / 2-10 ms / 10-30 ms / >=30 ms.
	 * pend_max: peak observed ESB ACK-payload FIFO depth since boot —
	 *   if this climbs, Hub is bursting queues beyond steady-state.
	 * ack_drop: TX_SUCCESS events with an empty ring (shouldn't happen
	 *   in principle; if nonzero we have a bookkeeping bug).
	 * See docs/RF_SYNC_DECISION_LOG.md Stage 1. */
	snprintk(line,
		 sizeof(line),
		 "SUMMARY role=central rx=%u anchors=%u tracked=%u usb_lines=%u err=%u hb=%u trigs=%u ack_lat=%u/%u/%u/%u pend_max=%u ack_drop=%u\n",
		 g_helixMocapStatus.rx_packets,
		 g_helixMocapStatus.anchors_sent,
		 g_helixMocapStatus.tracked_nodes,
		 g_helixMocapStatus.usb_lines,
		 g_helixMocapStatus.last_error,
		 g_helixMocapStatus.heartbeat,
		 ota_triggers_sent,
		 ack_tx_latency_bucket[0], ack_tx_latency_bucket[1],
		 ack_tx_latency_bucket[2], ack_tx_latency_bucket[3],
		 anchor_pending_max,
		 ack_tx_dropped_no_queue);
#else
	/* anchor_age buckets: how stale the most-recent anchor is vs. Tag's
	 * most-recent TX (proxy for Hub's ACK-path fast/slow distribution).
	 * offset_step buckets: |Δoffset| on each anchor (detects stale/wrong-
	 * Tag anchor corruption). See docs/RF_SYNC_DECISION_LOG.md */
	snprintk(line,
		 sizeof(line),
		 "SUMMARY role=node id=%u tx_ok=%u tx_fail=%u anchors=%u offset_us=%d err=%u hb=%u "
		 "anchor_age=%u/%u/%u/%u offset_step=%u/%u/%u/%u\n",
		 g_node_id,
		 g_helixMocapStatus.tx_success,
		 g_helixMocapStatus.tx_failed,
		 g_helixMocapStatus.anchors_received,
		 g_helixMocapStatus.estimated_offset_us,
		 g_helixMocapStatus.last_error,
		 g_helixMocapStatus.heartbeat,
		 anchor_age_bucket[0], anchor_age_bucket[1],
		 anchor_age_bucket[2], anchor_age_bucket[3],
		 offset_step_bucket[0], offset_step_bucket[1],
		 offset_step_bucket[2], offset_step_bucket[3]);
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

	struct esb_payload ack = {
		.pipe = payload->pipe,
	};

	/* Build anchor. When an OTA trigger is active, FLOOD every ACK
	 * payload with OTA_REQ + target node_id (not just ACKs going to the
	 * target). Previously we only tagged the ACK for the target Tag —
	 * but with N Tags sharing pipe 0 and heavy collisions, the target's
	 * ACK-carrying frames were the least likely to land, so triggers
	 * dropped ~50% first try (Copilot analysis, Apr 2026). Flooding
	 * every ACK means every Tag that does TX will see OTA_REQ in its
	 * next returned anchor and filter by target_node_id in firmware.
	 *
	 * retries now counts TOTAL flooded anchors sent (not per-target
	 * hits) — gives a time-bounded flood window regardless of which
	 * Tag happens to be RX'd. */
	const bool flooding = (ota_trigger_target_node != 0u);

	struct HelixSyncAnchor anchor = {
		.type = HELIX_PACKET_SYNC_ANCHOR,
		.central_id = (uint8_t)CONFIG_HELIX_MOCAP_CENTRAL_ID,
		.anchor_sequence = next_anchor_sequence++,
		.session_tag = (uint8_t)CONFIG_HELIX_MOCAP_SESSION_TAG,
		.central_timestamp_us = rx_timestamp_us,
		.flags = static_cast<uint8_t>(flooding ? HELIX_ANCHOR_FLAG_OTA_REQ : 0),
		.ota_target_node_id = flooding ? ota_trigger_target_node : (uint8_t)0,
	};
	ack.length = sizeof(anchor);
	memcpy(ack.data, &anchor, sizeof(anchor));
	/* Push this anchor's queue time onto the FIFO ring so the matching
	 * ESB_EVENT_TX_SUCCESS can compute a per-anchor latency. See
	 * docs/RF_SYNC_DECISION_LOG.md Stage 1 instrumentation. */
	{
		const uint32_t head = anchor_queue_ring_head;
		anchor_queue_ts_ring[head & HELIX_ANCHOR_QUEUE_MASK] = rx_timestamp_us;
		anchor_queue_ring_head = head + 1u;
		const uint32_t pending = (head + 1u) - anchor_queue_ring_tail;
		if (pending > anchor_pending_max) {
			anchor_pending_max = pending;
		}
	}
	if (esb_write_payload(&ack) == 0) {
		g_helixMocapStatus.anchors_sent++;
		if (flooding) {
			ota_triggers_sent++;
			if (ota_trigger_retries > 0u) {
				ota_trigger_retries--;
			} else {
				ota_trigger_target_node = 0u; /* flood window done */
			}
		}
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
	frame->node_id = g_node_id;
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
	if (payload->length < 1) {
		return;
	}

	/* Check for OTA trigger packet. This runs in ESB ISR context —
	 * just set the flag; the main loop does the actual reboot. */
	if (payload->data[0] == HELIX_PACKET_OTA_TRIGGER &&
	    payload->length >= sizeof(HelixOtaTrigger)) {
		const struct HelixOtaTrigger *trig =
			(const struct HelixOtaTrigger *)payload->data;
		if (trig->session_tag == (uint8_t)CONFIG_HELIX_MOCAP_SESSION_TAG &&
		    trig->magic == kOtaTriggerMagic &&
		    (trig->target_node_id == 0xFFu ||
		     trig->target_node_id == g_node_id)) {
			atomic_set(&ota_reboot_pending, 1);
		}
		return;
	}

	const struct HelixSyncAnchor *anchor = (const struct HelixSyncAnchor *)payload->data;
	const uint32_t local_us = now_us();

	/* Anchor wire format has evolved; accept the three sizes on the wire:
	 *   v1 (8 B):  base fields only, no flags, no OTA trigger path.
	 *   v2 (9 B):  + flags byte (OTA_REQ bit). Any OTA_REQ → reboot.
	 *   v3 (10 B): + ota_target_node_id — filter by our node_id.
	 * sizeof(HelixSyncAnchor) is 10 (v3) on this branch; the size gates
	 * below dispatch to the right compat path. */
	constexpr size_t kLegacyAnchorSize = 8u;
	constexpr size_t kV2AnchorSize     = 9u;
	static_assert(sizeof(HelixSyncAnchor) == 10u,
	              "v3 anchor must be 10 bytes — update OTA trigger compat paths if this grows");
	if (payload->length < kLegacyAnchorSize ||
	    anchor->type != HELIX_PACKET_SYNC_ANCHOR ||
	    anchor->session_tag != (uint8_t)CONFIG_HELIX_MOCAP_SESSION_TAG) {
		return;
	}

	/* Handle OTA trigger (piggybacked on anchor flags).
	 *  - 9-byte anchor (legacy): flag set → reboot (single-target semantics
	 *    because old Hubs only tagged the target's ACK).
	 *  - 10-byte anchor (v3+): Hub floods the flag on *every* ACK, so we
	 *    must filter by the ota_target_node_id field. 0xFF = broadcast
	 *    (reboot all). Own node_id (runtime, flash-provisioned) = targeted
	 *    reboot. Any other value = not for us, ignore. */
	if (!(anchor->flags & HELIX_ANCHOR_FLAG_OTA_REQ)) {
		/* no trigger bit — fast path */
	} else if (payload->length >= sizeof(*anchor)) {
		/* v3+ anchor — filter by target */
		if (anchor->ota_target_node_id == g_node_id ||
		    anchor->ota_target_node_id == 0xFFu) {
			atomic_set(&ota_reboot_pending, 1);
		}
	} else if (payload->length >= kV2AnchorSize) {
		/* legacy 9-byte anchor — no target field, legacy semantics */
		atomic_set(&ota_reboot_pending, 1);
	}

	/* Stage 1 instrumentation (docs/RF_SYNC_DECISION_LOG.md):
	 * Record how stale the anchor is vs. our most recent TX. A tight
	 * distribution near 0 = Hub hits the TIFS fast path. A long tail
	 * at 20 ms+ = Hub is mostly on the slow path (anchor rode a later
	 * frame's ACK). Use last_tx_local_us captured in maybe_send_frame. */
	const uint32_t tx_us_snapshot = last_tx_local_us;
	if (tx_us_snapshot != 0u && local_us >= tx_us_snapshot) {
		(void)histo_bucket_us(local_us - tx_us_snapshot, anchor_age_bucket);
	}

	const int32_t prev_offset = estimated_offset_us;
	estimated_offset_us = (int32_t)(local_us - anchor->central_timestamp_us);
	g_helixMocapStatus.estimated_offset_us = estimated_offset_us;
	g_helixMocapStatus.anchors_received++;
	have_anchor = true;

	/* Bucket the magnitude of the offset jump on this anchor. Small steps
	 * → clock is tracking smoothly; large steps → stale/wrong-Tag anchor
	 * corrupted the estimator (the shared-pipe cross-contamination
	 * symptom Stage 2 v4 anchor will fix). */
	const int64_t diff = (int64_t)estimated_offset_us - (int64_t)prev_offset;
	const uint32_t diff_abs = (uint32_t)((diff < 0) ? -diff : diff);
	(void)histo_bucket_us(diff_abs, offset_step_bucket);
}
#endif

static void event_handler(struct esb_evt const *event)
{
	switch (event->evt_id) {
	case ESB_EVENT_TX_SUCCESS:
		g_helixMocapStatus.tx_success++;
#if defined(CONFIG_HELIX_MOCAP_BRIDGE_ROLE_CENTRAL)
		/* Hub (PRX) TX_SUCCESS fires when a queued ACK payload (the
		 * anchor built in central_handle_frame) has been successfully
		 * transmitted. Pull the OLDEST queue timestamp off the ring —
		 * that's the anchor this TX_SUCCESS just delivered — and
		 * bucket the latency. Ring semantics: head is bumped on each
		 * enqueue in central_handle_frame; tail advances here on each
		 * successful TX. Both run from the same ESB ISR context in
		 * NCS v3.2.4, so no locking. */
		{
			const uint32_t tx_done_us = now_us();
			const uint32_t tail = anchor_queue_ring_tail;
			const uint32_t head = anchor_queue_ring_head;
			if (tail != head) {
				const uint32_t queue_us =
					anchor_queue_ts_ring[tail & HELIX_ANCHOR_QUEUE_MASK];
				anchor_queue_ring_tail = tail + 1u;
				if (tx_done_us >= queue_us) {
					(void)histo_bucket_us(tx_done_us - queue_us,
					                      ack_tx_latency_bucket);
				}
			} else {
				ack_tx_dropped_no_queue++;
			}
		}
#endif
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
	/* Per-Tag retransmit_delay spread so when two Tags collide their
	 * retries land at different times. 600 µs base + 50 µs × node_id
	 * gives 650 µs (node 1) through 1100 µs (node 10) — 450 µs spread.
	 * Hub (central) stays at the base value because PRX doesn't retry
	 * on collision anyway. */
#if defined(CONFIG_HELIX_MOCAP_BRIDGE_ROLE_NODE)
	config.retransmit_delay = static_cast<uint16_t>(600u + (uint16_t)g_node_id * 50u);
#else
	config.retransmit_delay = 600;
#endif
	/* retransmit_count 6 → 10: with 10 Tags on pipe 0 a chunk is up
	 * against ~9 other Tags for any given air slot; more retries
	 * compensates for the elevated collision rate (Phase A baseline
	 * showed 38-46 Hz / Tag vs 50 Hz target, i.e. 12-24 % loss). */
	config.retransmit_count = 10;
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
	} else {
		/* Record the TX instant for later anchor-age pairing. Simple
		 * "last TX" (not a seq-indexed ring) — sufficient for Stage 1
		 * instrumentation because each Tag TXes at a known cadence and
		 * anchors typically arrive within 1-2 TX periods. See
		 * docs/RF_SYNC_DECISION_LOG.md. Stage 3 (v5 anchor) will move
		 * to a seq-indexed ring so the midpoint estimator can pair a
		 * delayed anchor with the correct TX event. */
		last_tx_local_us = now_us();
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

static volatile bool ota_commit_pending;

/* Deferred BEGIN: flash erase takes ~10s and would block the BLE stack
 * if run inside the GATT callback, causing supervision timeout. Instead,
 * we save the BEGIN parameters and process them in the main OTA loop. */
static volatile bool ota_begin_pending;
static uint8_t ota_begin_buf[16];
static uint16_t ota_begin_len;

static ssize_t ota_write_ctrl(struct bt_conn *, const struct bt_gatt_attr *,
                              const void *buf, uint16_t len, uint16_t, uint8_t)
{
	const auto *data = static_cast<const uint8_t *>(buf);

	/* Defer BEGIN (cmd=0x01) to the main loop — flash erase is too slow
	 * to run inside the GATT callback. But validate payload *here* so we
	 * can NACK immediately on bad target_id or short payload; otherwise
	 * the uploader would think BEGIN succeeded and poll STATUS forever
	 * while the main-loop handleControlWrite silently rejects. */
	if (len > 0 && data[0] == helix::BleOtaService::CMD_BEGIN) {
		if (len < helix::BleOtaService::kCtrlBeginMinLen ||
		    len > sizeof(ota_begin_buf)) {
			return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
		}
		/* Validate target_id before accepting.
		 * Layout: [cmd:1][size:4][crc:4][tid:4] — target_id at offset 9..12.
		 * kCtrlBeginMinLen is 13, so the len check above guarantees data[9..12]
		 * are in bounds; static_assert pins the relationship so a future
		 * refactor can't silently break it. */
		static_assert(helix::BleOtaService::kCtrlBeginMinLen >= 13u,
		              "BEGIN payload must be at least 13 bytes for target_id at offset 9..12");
		uint32_t req_tid;
		memcpy(&req_tid, &data[9], sizeof(req_tid));
		if (req_tid != (uint32_t)CONFIG_HELIX_OTA_TARGET_ID) {
			/* Audible failure. Previously this returned silently; if an
			 * uploader ever targets a mixed-firmware fleet with the wrong
			 * --target-id, the only symptom was a GATT NACK from here
			 * with no clue which side was off. */
			printk("ota: BEGIN rejected — target_id 0x%08x != expected 0x%08x\n",
			       (unsigned)req_tid,
			       (unsigned)CONFIG_HELIX_OTA_TARGET_ID);
			return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
		}
		memcpy(ota_begin_buf, data, len);
		ota_begin_len = len;
		ota_begin_pending = true;
		return static_cast<ssize_t>(len); /* ACK, main loop does erase */
	}

	auto status = ota_service.handleControlWrite(data, len);
	if (status == helix::OtaStatus::OK &&
	    ota_manager.state() == helix::OtaState::COMMITTED) {
		ota_commit_pending = true;
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

	/* Pre-bt_enable settle delay. On fast back-to-back reboots (OTA +
	 * warm-ish NVIC reset), the SoftDevice Controller's LF clock /
	 * MPSL calibration hasn't re-converged, and the first connection
	 * after bt_enable() drops with supervision-timeout (reason=8)
	 * within ~3 s — consistent with LFRC drift accumulating to a full
	 * connection-interval miss. 2 s fixed 4/5 cycles; cycle 5 failed
	 * the same way deterministically (including an automatic retry),
	 * suggesting the die has warmed across prior OTAs and LFRC has
	 * drifted further from the cold-boot calibration point. 4 s gives
	 * MPSL enough time to re-converge even on a warm chip. */
	k_sleep(K_MSEC(4000));

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

	/* Read our own MCUboot image version from the active slot */
	struct mcuboot_img_header img_hdr = {};
	uint8_t app_major = 0, app_minor = 0;
	uint16_t app_rev = 0;
	uint32_t app_build = 0;
	if (boot_read_bank_header(FIXED_PARTITION_ID(slot0_partition),
	                          &img_hdr, sizeof(img_hdr)) == 0 &&
	    img_hdr.mcuboot_version == 1) {
		app_major = img_hdr.h.v1.sem_ver.major;
		app_minor = img_hdr.h.v1.sem_ver.minor;
		app_rev = img_hdr.h.v1.sem_ver.revision;
		app_build = img_hdr.h.v1.sem_ver.build_num;
	}
	printk("ota: app v%u.%u.%u+%u\n", app_major, app_minor, app_rev, app_build);

	/* Build advertising data — use static storage so pointers persist */
	const char *bt_name = bt_get_name();
	static uint8_t adv_flags[] = {BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR};

	/* Manufacturer-specific data carrying versions.
	 * Layout: [company_id_lo][company_id_hi] [tag='H'][payload_ver=1]
	 *         [app_major][app_minor][app_rev_lo][app_rev_hi]
	 *         [boot_major][boot_minor][boot_rev_lo][boot_rev_hi]
	 * Total: 12 bytes payload (build_num dropped for BLE fit).
	 * Adv budget: flags(3) + name "HTag-XXXX"(11) + mfg(14) = 28 / 31 bytes. */
	static uint8_t mfg_data[12];
	mfg_data[0] = 0xFF; mfg_data[1] = 0xFF;  /* local-use company id */
	mfg_data[2] = 'H';                        /* magic — 'H'elix advert marker */
	mfg_data[3] = 0x01;                       /* payload version */
	mfg_data[4] = app_major;
	mfg_data[5] = app_minor;
	mfg_data[6] = app_rev & 0xFF;
	mfg_data[7] = (app_rev >> 8) & 0xFF;
	mfg_data[8]  = CONFIG_HELIX_BOOTLOADER_VERSION_MAJOR;
	mfg_data[9]  = CONFIG_HELIX_BOOTLOADER_VERSION_MINOR;
	mfg_data[10] = CONFIG_HELIX_BOOTLOADER_VERSION_REVISION & 0xFF;
	mfg_data[11] = (CONFIG_HELIX_BOOTLOADER_VERSION_REVISION >> 8) & 0xFF;

	static struct bt_data ad[3];
	ad[0] = {BT_DATA_FLAGS, sizeof(adv_flags), adv_flags};
	ad[1] = {BT_DATA_NAME_COMPLETE, static_cast<uint8_t>(strlen(bt_name)),
	         reinterpret_cast<const uint8_t *>(bt_name)};
	ad[2] = {BT_DATA_MANUFACTURER_DATA, sizeof(mfg_data), mfg_data};

	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, 3, nullptr, 0);
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
	 * Commit is deferred from the GATT callback to avoid flash writes
	 * interfering with BLE timing. The ota_commit_pending flag signals
	 * that commit succeeded and we should reboot.
	 *
	 * Three timeout guards:
	 *   1. Stall detector (existing): 30 s of no byte progress while in
	 *      RECEIVING state → force disconnect.
	 *   2. Idle-connection timeout (new, 60 s): peer connected but never
	 *      sent BEGIN (state stays IDLE). Prevents the Tag getting stuck
	 *      forever when a phantom connection never sees its disconnect
	 *      callback fire (observed on the J-Link-attached Tag where SWD
	 *      halt events suppressed MPSL's link-layer teardown — but can
	 *      also hit any Tag if the peer vanishes at the wrong moment).
	 *   3. Hard session ceiling (new, 300 s): no single OTA session should
	 *      ever exceed 5 minutes regardless of state. Belt-and-braces for
	 *      the case where ota_connected is stuck true but neither the
	 *      stall nor idle guard fires (e.g. oscillating between states).
	 * Any of these triggers bt_conn_disconnect which clears ota_connected
	 * via the disconnect callback; the loop then exits cleanly to ESB.
	 * If the disconnect callback itself is ever suppressed, loop still
	 * exits because we `break` immediately after issuing the disconnect. */
	printk("ota: running OTA transfer\n");
	const int64_t session_start = k_uptime_get();
	uint32_t last_bytes = 0;
	uint32_t stall_ticks = 0;
	uint32_t idle_ticks = 0;
	while (ota_connected) {
		/* Deferred BEGIN: run flash erase in the main loop where it
		 * won't block the BLE stack's connection event processing. */
		if (ota_begin_pending) {
			printk("ota: begin (deferred erase)\n");
			auto status = ota_service.handleControlWrite(ota_begin_buf, ota_begin_len);
			printk("ota: begin status=%d\n", static_cast<int>(status));
			ota_begin_pending = false;
		}
		if (ota_commit_pending) {
			printk("ota: commit done, rebooting\n");
			k_sleep(K_MSEC(CONFIG_HELIX_OTA_REBOOT_DELAY_MS));
			sys_reboot(SYS_REBOOT_COLD);
		}
		/* Hard session ceiling — 300 s max. */
		if ((k_uptime_get() - session_start) > 300000LL) {
			printk("ota: session ceiling (300s) hit, disconnecting\n");
			if (ota_conn) {
				bt_conn_disconnect(ota_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
			}
			break;
		}
		uint32_t bytes = ota_manager.bytesReceived();
		const auto ota_state = ota_manager.state();
		if (ota_state == helix::OtaState::RECEIVING) {
			idle_ticks = 0;
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
		} else if (ota_state == helix::OtaState::IDLE && !ota_begin_pending) {
			/* Idle-connection guard — peer connected but never sent
			 * a BEGIN (or disconnect callback never fired on a
			 * phantom peer). */
			if (++idle_ticks >= 1200) { /* 1200 * 50ms = 60s */
				printk("ota: idle connection (60s no BEGIN), disconnecting\n");
				if (ota_conn) {
					bt_conn_disconnect(ota_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
				}
				break;
			}
		} else {
			idle_ticks = 0;
		}
		led_set(((k_uptime_get() / 250) & 1) != 0); /* medium blink = OTA active */
		k_sleep(K_MSEC(50));
	}
	/* Check commit after disconnect too (commit might have set flag just as connection dropped) */
	if (ota_commit_pending) {
		printk("ota: commit done (post-disconnect), rebooting\n");
		k_sleep(K_MSEC(CONFIG_HELIX_OTA_REBOOT_DELAY_MS));
		sys_reboot(SYS_REBOOT_COLD);
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

	/* Resolve node_id from flash first so every subsequent subsystem
	 * (ESB init, logging, OTA trigger filter) sees the provisioned value. */
	g_node_id = helix_load_node_id();

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

#if defined(CONFIG_HELIX_OTA_HUB_RELAY)
	ota_hub_relay_init(host_stream);
#endif

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

	/* ── Release BLE/MPSL before ESB ──────────────────── */
#if defined(CONFIG_BT)
	/* Both Hub and Tag need bt_enable()+bt_disable() before esb_init()
	 * to properly cycle MPSL resources. The SoftDevice Controller's
	 * SYS_INIT claims TIMER0/RTC0/RADIO. bt_disable() with
	 * CONFIG_BT_UNINIT_MPSL_ON_DISABLE releases them for ESB. */
#if !defined(CONFIG_HELIX_MOCAP_BRIDGE_ROLE_NODE) || \
    !(CONFIG_HELIX_OTA_BOOT_WINDOW_MS > 0)
	{
		int bt_err = bt_enable(nullptr);
		if (bt_err == 0 || bt_err == -EALREADY) {
			bt_disable();
			printk("ble/mpsl released for esb\n");
		}
	}
#endif
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

#if defined(CONFIG_HELIX_MOCAP_BRIDGE_ROLE_NODE)
	/* Per-Tag boot-time TX phase offset. All 10 Tags leaving the 300 s
	 * BLE OTA window at slightly different wallclocks naturally spread
	 * their TX phase, but when everyone reaches the main loop near the
	 * same instant (e.g. after a synchronised Hub reset) they tend to
	 * collide in lockstep. A one-off 200 µs × node_id sleep here gives
	 * an initial spread of 200..2000 µs across the fleet. LF clock
	 * differences will drift it over time, but this gets us started
	 * clean. Measured Phase A baseline: 38-46 Hz/Tag; target is 50 Hz. */
	if (g_node_id > 0u) {
		k_usleep((uint32_t)g_node_id * 200u);
	}
#endif

	static bool esb_running = true;

	while (1) {
		heartbeat_tick();

#if defined(CONFIG_HELIX_OTA_HUB_RELAY)
		/* Check if OTA relay needs to stop ESB */
		if (esb_running && ota_hub_relay_needs_esb_stop()) {
			printk("hub: stopping ESB for OTA relay\n");
			esb_disable();
			k_sleep(K_MSEC(100)); /* let radio peripheral settle */
			esb_running = false;
		}

		/* Poll OTA relay (processes USB RX, drives BLE state machine) */
		ota_hub_relay_poll();

		/* Check if OTA session ended and ESB can restart */
		if (!esb_running && ota_hub_relay_esb_can_restart()) {
			printk("hub: restarting ESB after OTA relay\n");
			esb_initialize();
#if defined(CONFIG_HELIX_MOCAP_BRIDGE_ROLE_CENTRAL)
			esb_start_rx();
#endif
			esb_running = true;
			host_write("HELIX_MOCAP_BRIDGE_READY\n");
		}
#endif

#if defined(CONFIG_HELIX_MOCAP_BRIDGE_ROLE_NODE)
		/* Handle deferred OTA reboot triggered from ESB ISR */
		if (atomic_cas(&ota_reboot_pending, 1, 0)) {
			printk("ota: ESB trigger received, rebooting\n");
			k_sleep(K_MSEC(50));
			sys_reboot(SYS_REBOOT_COLD);
		}
		maybe_send_frame();
#endif
		if ((g_helixMocapStatus.heartbeat % 25U) == 0U) {
			report_summary();
		}
		k_sleep(K_MSEC(CONFIG_HELIX_MOCAP_SEND_PERIOD_MS));
	}

	return 0;
}
