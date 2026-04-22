# nRF Hub-Relay BLE OTA (current primary workflow)

This is the supported production OTA path on `nrf-xiao-nrf52840` as of
2026-04-20. It supersedes the legacy direct-BLE workflow documented in
[`NRF_BLE_OTA_WORKFLOW.md`](NRF_BLE_OTA_WORKFLOW.md) — that doc described
a standalone OTA-only app and a PC-dongle-as-BLE-central uploader; the
current app is the merged mocap bridge and the BLE central role is held
by the Hub (nRF52840), reached from the PC over USB CDC.

## Architecture

```
PC  ──USB CDC──►  Hub (nRF52840)  ──BLE──►  Tag (nRF52840)
                    │                           │
                 ESB PRX                    ESB PTX (normal)
                                            BLE peripheral (OTA window)
```

Single Tag firmware image. Each Tag's identity (`node_id`) is flash-
provisioned once over SWD into the first word of the settings_storage
partition; see `Provisioning` below. OTA never touches that word.

Repo files:

- Tag + Hub firmware: `zephyr_apps/nrf52840-mocap-bridge/`
- Hub-relay module: `zephyr_apps/nrf52840-mocap-bridge/src/ota_hub_relay.cpp`
- PC uploader: `tools/nrf/hub_ota_upload.py`
- Provisioning helper (SWD): `tools/nrf/flash_tag.sh`
  (upstreamed from scratch in commit addressing `docs/RF.md`;
  accepts env-var overrides for J-Link serial, artifact dir, merged-hex path)

## End-to-end flow

1. PC opens `/dev/ttyACM1` (Hub CDC).
2. PC sends `EsbTrigReq` (`0x50`) with `[node_id, retries=100]` to Hub.
3. Hub (in ESB PRX mode) sets `ota_trigger_target_node = node_id` and
   `ota_trigger_retries = 100`. While the trigger is active, Hub FLOODS
   every ACK payload (anchor) with `OTA_REQ` + `target_node_id` —
   not just the ACKs going to the target. See **Root cause / flood fix**
   below.
4. Each Tag, on every received anchor, checks `ota_target_node_id`
   against its flash-provisioned `g_node_id`. Target matches → Tag
   sets `ota_reboot_pending` flag. Main loop reboots.
5. Target Tag boots, hits 4 s `k_sleep` to let the LF clock / MPSL
   re-converge, then `bt_enable()` + advertises as `HTag-XXXX` (the
   suffix is derived from FICR `DEVICEADDR`).
6. Meanwhile the uploader sleeps 8 s post-trigger (cov ers Tag boot +
   LFRC settle + advert start), then sends `InfoReq` with the target
   name over CDC.
7. Hub receives `InfoReq`, disables ESB, waits 4 s for its own LF
   clock / MPSL settle, sends `InfoRsp`, `bt_enable()`, and starts a
   BLE scan filtered by the name prefix.
8. Hub connects to the Tag, does ATT MTU exchange
   (`CONFIG_BT_GATT_AUTO_UPDATE_MTU=y`), requests 7.5–30 ms connection
   interval, and discovers the Helix OTA GATT service.
9. Uploader streams the signed firmware image in 128-byte data chunks.
   Hub forwards each chunk via `bt_gatt_write()` (write-with-response)
   and replies to the uploader with `DataRsp` (per-chunk ACK) so the
   Hub's USB CDC RX ring doesn't overflow.
10. After the last chunk, uploader sends `CtrlWrite(COMMIT)`. Tag
    calls `boot_request_upgrade(BOOT_UPGRADE_PERMANENT)` and reboots.
11. MCUboot validates the signed image (ECDSA P-256) and swaps into
    the primary slot (overwrite-only — swap-using-move is a future
    task). Tag boots the new app, re-enters the OTA window briefly,
    then falls through to ESB.
12. Hub sees the BLE disconnect, restarts ESB, prints
    `HELIX_MOCAP_BRIDGE_READY`; uploader sees that line and reports
    success.

## Four reliability fixes stacked on this branch

Commit history (most-recent first):

| Commit | Fix | Impact |
|---|---|---|
| `f2fd101` | **ESB trigger flood + `ota_target_node_id` filter** | Trigger delivery ~50 % → 100 % on a 10-Tag fleet (20/20 verified) |
| `e6b0f17` | Flash-backed Tag `node_id` (single firmware, per-Tag provisioning) | Fleet uses one signed binary; `node_id` stored at `0xFE000` |
| `b35bab5` | 4 s `k_sleep` before `bt_enable`, pre-`InfoRsp` ordering | Eliminated cumulative-reboot supervision-timeout; 5/5 back-to-back |
| `b8201fe` | `CONFIG_BT_GATT_AUTO_UPDATE_MTU=y` + per-chunk `DataRsp` | Stopped Hub's ATT writes being rejected as "not supported"; stopped USB CDC RX overflow that dropped ~1.7 MB per OTA |

### Root cause / flood fix (f2fd101)

With 10 Tags all transmitting on ESB pipe 0 at 50 Hz, aggregate ~500
frames/s; Hub processes ~30–50 / s after collisions. The old trigger
only set `OTA_REQ` on the ACK going to the target node — which meant
Hub had to successfully RX a frame *from the target* in the 4 s trigger
window. With 10-Tag collisions that succeeded ~50 % first try.

The flood fix: set `OTA_REQ` on **every** ACK payload during the
trigger window, and put the target `node_id` in the anchor so each Tag
can filter in firmware. Hub now wins every RX it gets, no matter which
Tag it came from. Observed improvement: 24/30 → 20/20 (100 %).

## Anchor wire format (v3, 10 bytes)

```c
struct __packed HelixSyncAnchor {
    uint8_t  type;                   // 0xA1
    uint8_t  central_id;
    uint8_t  anchor_sequence;
    uint8_t  session_tag;
    uint32_t central_timestamp_us;
    uint8_t  flags;                  // bit 0 = OTA_REQ
    uint8_t  ota_target_node_id;     // filter byte — target node_id or 0xFF broadcast
};
```

A v3 Tag accepts v3 (10 B), v2 (9 B, no `ota_target_node_id` — legacy
"any OTA_REQ is for me"), and v1 (8 B, no flags at all — no OTA
trigger path). Migration cost from v2 → v3 is one round of mass Tag
reboot on the flood, since v2 Tags don't filter.

## Provisioning

Flash layout for `promicro_nrf52840/nrf52840`:

```
0x00000      mcuboot          (~48 KB)
0x0C000      mcuboot_primary  (484 KB)    signed app lives here
0x85000      mcuboot_secondary (484 KB)   OTA stages here
0xFE000      settings_storage  (8 KB)     node_id at word 0
                                          CONFIG_SETTINGS=n, unused otherwise
0x100000     end of flash
```

To flash a Tag with the current merged image and provision its
`node_id`:

```bash
SN=<jlink-serial>
ID=<node_id 1..255>
HEX=.deps/ncs/v3.2.4/build-helix-promicro_nrf52840-nrf52840-mocap-node-node1/merged.hex
nrfutil device program --firmware "$HEX" --serial-number "$SN" \
                       --options verify=VERIFY_READ
# NO --direct — direct bypasses NVMC setup and the flash write is
# silently dropped.
nrfutil device write --serial-number "$SN" --address 0xFE000 \
                     --value $(printf "0xFFFFFF%02X" "$ID")
nrfutil device reset --serial-number "$SN"
```

Read-back after provisioning: the config word should be `0xFFFFFF<NN>`
where `NN` is the node_id (hex). Field reassignment later is the same
sequence minus the re-flash — `nrfutil device write` rewrites the
single word.

Fallback: if the word is `0xFFFFFFFF` (unprovisioned), firmware uses
`CONFIG_HELIX_MOCAP_NODE_ID` from Kconfig (default 1), so dev builds
on one Tag still work without provisioning.

## OTA Commands (one Tag)

Assuming the Tag is already on firmware with a valid `node_id`:

```bash
nix develop --command bash -lc \
    'tools/nrf/build_mocap_bridge.sh node promicro_nrf52840/nrf52840 1'
nrfutil device reset --serial-number <hub-sn>   # fresh Hub
sleep 8
python3 tools/nrf/hub_ota_upload.py \
    .deps/ncs/v3.2.4/build-helix-promicro_nrf52840-nrf52840-mocap-node-node1/nrf52840-mocap-bridge/zephyr/zephyr.signed.bin \
    --port /dev/ttyACM1 --target HTag-<SUFFIX> --trigger-node <node_id>
```

Expected wall time: ~240 s per Tag.

## Fleet OTA (all 10 Tags, one firmware version bump per round)

The reliability harness at `tools/nrf/fleet_ota.sh` iterates across the
10 Tags, bumping `CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION` each round and
rebuilding once per round. Logs to
`${HELIX_RF_ARTIFACT_DIR:-/tmp/helix_tag_log}/fleet_ota.log` and a
summary to `fleet_ota_summary.txt` in the same directory.

Verified result (2026-04-20, branch `nrf-xiao-nrf52840` at `f2fd101`):

| Round | Build | Starting state | Result |
|---|---|---|---|
| R1 | → v6 | fleet mid-migration (mixed v4/v5 firmware) | 10/10 (9 first-try, 1 retry) |
| R2 | → v7 | fleet entirely on flood-aware firmware | **10/10 first-try** |

## Failure modes and what each looks like

| Symptom in uploader | Meaning | Fix / debug |
|---|---|---|
| `ERROR: InfoReq failed` | Hub didn't come back from USB reset in time | Increase pre-`InfoReq` sleep (currently 8 s) |
| `ERROR: StatusReq failed (Hub may not have connected)` in ~20 s | ESB trigger didn't reach the Tag (most common pre-`f2fd101`); Hub finished BLE scan without finding target | Confirm `f2fd101` is on both Tag and Hub. Read Hub state via SWD if needed. |
| `ERROR: Tag did not transition to RECEIVING after 45 s` | Tag's `ota_begin_pending` was set but main loop didn't run `OtaManager::begin`; typically MPSL/LFRC not settled | Check Tag is on `b35bab5`+ (pre-`bt_enable` k_sleep) |
| `ERROR: DATA_RSP err=0x06` | Hub wrote to GATT with MTU below chunk size (ATT_ERR_NOT_SUPPORTED) | Confirm `CONFIG_BT_GATT_AUTO_UPDATE_MTU=y` on Hub |
| `ERROR: DATA_RSP err=0x13` mid-transfer | Tag's `OtaManager::writeChunk` returned BAD_OFFSET — some chunks were lost before reaching the Tag | Check `hub_rx_drops` (re-enable debug counter if removed); indicates USB CDC overflow, revisit back-pressure |
| `COMMIT response timeout (expected)` | Normal — Tag reboots before the Hub can forward the `CtrlRsp` | Not a failure |
| `WARNING: Hub didn't confirm ESB restart` | Uploader's 30 s post-COMMIT window saw neither `HELIX_MOCAP_BRIDGE_READY` nor `FRAME` bytes | Not always a failure — verify slot0 via SWD if possible. Most common when Hub's ESB restart line lands just outside the window. |
