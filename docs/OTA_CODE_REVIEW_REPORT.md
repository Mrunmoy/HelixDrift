# OTA Code Review Report — Last 10 Commits (m8 series)

**Scope:** `4c53235..3072d4c` (10 commits)
**Date:** 2026-04-20
**Reviewer:** Copilot (Claude Opus 4.6)
**Branch:** `nrf-xiao-nrf52840`

---

## Executive Summary

The m8 commit series addresses OTA reliability across four areas:
1. Flash partition mismatch (PM vs DTS) — **fixed correctly**
2. ESB OTA trigger flooding — **fixed, minor issues remain**
3. BLE relay flow control (write-with-response + DATA_RSP) — **fixed, dead code left behind**
4. LFRC clock settle delay — **empirical fix, adequate**

Overall the changes are solid and well-reasoned. The findings below are
ranked by severity.

---

## FINDINGS

### 🔴 BUG — Stale comment + wrong size constant in anchor parsing (main.cpp:505–506)

**File:** `main.cpp:505–506`
**Commit:** `f2fd101`

```cpp
/* Accept both 8-byte (legacy, no flags) and 9-byte (flags) anchors.
 * sizeof(HelixSyncAnchor) is 9 in v2+, we check the legacy 8. */
constexpr size_t kLegacyAnchorSize = 8u;
```

`sizeof(HelixSyncAnchor)` is **10 bytes** (not 9): `type(1) + central_id(1) +
anchor_sequence(1) + session_tag(1) + central_timestamp_us(4) + flags(1) +
ota_target_node_id(1) = 10`. The struct is `__packed`.

The branching logic then does:

```cpp
if (payload->length >= sizeof(*anchor))      // ≥ 10 → v3+ anchor
else if (payload->length >= 9)               // ≥ 9 → legacy 9-byte
```

This is **semantically correct** because v2 anchors (flags but no target_id)
were 9 bytes and v3 anchors are 10 bytes. However:

- The comment says "sizeof(HelixSyncAnchor) is 9" — it's 10. **Fix the comment.**
- Consider adding `constexpr size_t kV2AnchorSize = 9u;` for readability.

**Impact:** Low (logic is correct, comment is misleading).

---

### 🔴 BUG — Race condition in `helix_request_ota_trigger()` (main.cpp:187–191)

**File:** `main.cpp:187–191`

```cpp
extern "C" void helix_request_ota_trigger(uint8_t node_id, uint8_t retries)
{
    ota_trigger_retries = retries;     // ← ISR can read target_node != 0
    ota_trigger_target_node = node_id; //   while retries is still the OLD value
}
```

This is called from the main-loop (relay handler) but consumed in the ESB RX
ISR (`central_handle_frame`). The ISR reads `ota_trigger_target_node` first
(line 421), and if it's still non-zero from a *previous* trigger that hasn't
expired yet, it will decrement the *old* `ota_trigger_retries` value.

Worse: between the two volatile writes, the ISR can fire and see
`ota_trigger_target_node` set to the **old** node while `retries` is the
**new** value.

**Fix:** Write `target_node` *first* (or use a single atomic store of a
packed `{node, retries}` pair), or simply write `target_node = 0` before
setting retries to gate the ISR:

```cpp
extern "C" void helix_request_ota_trigger(uint8_t node_id, uint8_t retries)
{
    ota_trigger_target_node = 0u;       // gate: ISR ignores during setup
    __DMB();                            // ensure ordering on Cortex-M4
    ota_trigger_retries = retries;
    __DMB();
    ota_trigger_target_node = node_id;  // arm
}
```

**Impact:** Medium — with 100 retries the window is narrow, but a stale
trigger from a previous round could cause an unintended reboot of the wrong
Tag.

---

### 🟡 ISSUE — Dead code: `data_tx_sem` and `data_tx_complete_cb` (ota_hub_relay.cpp:27–40)

**File:** `ota_hub_relay.cpp:27–40`

The binary semaphore `data_tx_sem` and its callback `data_tx_complete_cb` are
defined but **never referenced** anywhere else in the file. The DataWrite
handler was refactored from write-without-response (which would have used the
`_cb` variant) to write-WITH-response (`bt_gatt_write`), making these
obsolete.

**Fix:** Delete lines 27–40.

**Impact:** Low (dead code, no runtime effect, minor flash waste).

---

### 🟡 ISSUE — `ota_write_ctrl` hardcoded payload offsets without bounds check (main.cpp:702–706)

**File:** `main.cpp:702–706`

```cpp
uint32_t req_tid = (uint32_t)data[9]
                 | ((uint32_t)data[10] << 8)
                 | ((uint32_t)data[11] << 16)
                 | ((uint32_t)data[12] << 24);
```

The check above is `len < kCtrlBeginMinLen` (which is 13). So `data[12]` is
the last byte accessed. `kCtrlBeginMinLen = 13` makes indexes 0–12 valid.
**This is correct.** However, the magic numbers 9–12 are fragile. Consider
using `memcpy(&req_tid, &data[9], 4)` with a `static_assert` on
`kCtrlBeginMinLen >= 13` to make the relationship explicit and avoid
alignment issues on other platforms.

**Impact:** Low (correct today, fragile under refactor).

---

### 🟡 ISSUE — `now_us()` wraps at ~71.6 minutes (main.cpp:194–197)

**File:** `main.cpp:194–197`

```cpp
static uint32_t now_us(void)
{
    return (uint32_t)(k_uptime_get() * 1000LL);
}
```

`k_uptime_get()` returns milliseconds. `* 1000` converts to µs. A `uint32_t`
wraps at ~4295 seconds ≈ **71.6 minutes**. If a Tag runs ESB for >72 minutes
before an OTA trigger (or if a Hub runs continuously), timestamp sync will
wrap. The `estimated_offset_us` subtraction `local_us - anchor->central_timestamp_us`
is safe as long as **both** sides wrap at the same rate (which they do — both
are uint32_t). However, the raw `node_local_timestamp_us` field in
`HelixMocapFrame` becomes ambiguous after 72 min.

**Impact:** Low for OTA (irrelevant), medium for long-running mocap sessions.
Not introduced by these commits — pre-existing.

---

### 🟡 ISSUE — Double-blank-line at main.cpp:795–796

**File:** `main.cpp:795–796`

Trivial: the commit introduced an extra blank line between `img confirmed`
and `ota_backend.init()`.

**Impact:** None (cosmetic).

---

### 🟡 ISSUE — `mfg_data[2] = 'V'` marker byte change not documented (main.cpp:869)

**File:** `main.cpp:869`
**Commit:** `b35bab5`

```cpp
mfg_data[2] = 'V';  /* v4 marker byte changed H->V to prove swap */
```

This was a diagnostic change to prove MCUboot image swap works. The marker
should be reverted to `'H'` (the production value) or the advertising parser
in any companion PC tool needs to accept both `'H'` and `'V'`. If the fleet
test harness or any BLE scanner filters on `mfg_data[2] == 'H'`, it will
miss these Tags.

**Impact:** Medium — depends on whether upstream tools filter on this byte.

---

### 🟡 ISSUE — `hub_ota_upload.py` default `--target-id` changed silently (hub_ota_upload.py:110)

**File:** `tools/nrf/hub_ota_upload.py:110`
**Commit:** `e6b0f17`

```python
p.add_argument("--target-id", type=lambda v: int(v, 0), default=0x52840071,
```

The default changed from `0x52840070` to `0x52840071` without a corresponding
Kconfig change visible in the diff. This means the tool default and the
Tag's compiled `CONFIG_HELIX_OTA_TARGET_ID` must match. If any Tag still runs
firmware with `0x52840070`, the upload will fail silently (GATT NACK from the
new `ota_write_ctrl` validation).

**Fix:** Verify all deployed Tags use the same target-id. Consider logging
the target-id mismatch on the Tag side more visibly.

**Impact:** Medium in mixed-fleet scenarios.

---

### 🟢 OBSERVATION — 4 s LFRC settle delay appears in TWO places

**Files:**
- `main.cpp:814` — Tag's `run_ota_boot_window()` (before `bt_enable`)
- `ota_hub_relay.cpp:549` — Hub's `ota_hub_relay_poll()` (before `bt_enable`)

Both are 4000 ms. The Hub delay occurs in the `BLE_INIT` state, *before*
sending InfoRsp. The uploader's InfoRsp timeout is documented as 15 s, so
4 s is safe.

However, the total latency is now: **8 s uploader sleep + 4 s Hub LFRC + BLE
scan + connect ≈ 14–16 s** before data transfer starts. This is generous but
reasonable for reliability.

**No fix needed** — just noting the cumulative delay budget.

---

### 🟢 OBSERVATION — `OtaManager::begin()` now calls `abort()` on re-entry

**File:** `OtaManager.cpp:12`

Previously, calling `begin()` while already in `RECEIVING` state returned
`ERROR_INVALID_STATE`. Now it calls `abort()` first, allowing a fresh start.
This is the correct behaviour for retries after a dropped BLE connection.

The `abort()` call resets `state_` to `IDLE`, `nextExpectedOffset_` to 0, and
`crcAccum_` to the init value. The subsequent `eraseSlot()` call will
re-erase. **Correct and well-motivated.**

---

### 🟢 OBSERVATION — `ZephyrOtaFlashBackend` PM vs DTS fix is correct

**File:** `ZephyrOtaFlashBackend.cpp`
**Commit:** `4c53235`

The root cause of the "silent OTA" bug: `FIXED_PARTITION_ID(slot1_partition)`
resolved to the DTS address (0x82000) while MCUboot and imgtool used the PM
address (0x85000). OTA writes went to the wrong flash region; MCUboot never
saw a valid image.

The fix uses `PM_MCUBOOT_SECONDARY_ID` when available, with a fallback to
the DTS partition for non-PM builds. **Correct and well-guarded.**

---

### 🟢 OBSERVATION — Flash-backed node_id design is sound

**File:** `main.cpp:128–162`
**Commit:** `e6b0f17`

Direct flash read from `0xFE000` in the `settings_storage` partition. Guard
against unprogrammed flash (0xFFFFFFFF) and zero. Falls back to Kconfig
default. No write path — provisioning is SWD-only. OTA never touches this
page.

One minor concern: the `reinterpret_cast<const volatile uint32_t *>(0xFE000u)`
is a raw absolute address. If the partition map ever changes, this will
silently read garbage. Consider adding a build-time assertion:

```cpp
BUILD_ASSERT(PM_SETTINGS_STORAGE_ADDRESS == 0xFE000u,
             "node_id flash address must match settings_storage partition");
```

---

### 🔴 BUG — Phantom BLE connection hangs Tag forever (main.cpp:917–947)

**File:** `main.cpp:917–947`
**Discovered by:** Claude (separate session), confirmed in this review.

The `while (ota_connected)` inner loop has **no timeout for the IDLE state**.
The existing stall detector (line 932–943) only fires when
`ota_manager.state() == RECEIVING`. If a BLE peer connects but never sends
BEGIN, or if the disconnect callback is suppressed (SWD halt during MPSL
radio event, peer crash, link-layer teardown lost), the Tag loops forever
doing nothing but blinking the LED at 250 ms.

This is **not just a debug artifact** — any scenario where the disconnect
callback fails to fire (signal loss at the exact wrong moment, MPSL clock
jitter, BLE stack edge case) will trigger it. The 300 s OTA boot window
only gates the *advertising* wait; once `ota_connected` is set, the inner
loop has no ceiling.

The Tag keeps blinking after reflash + reboot because `run_ota_boot_window()`
runs unconditionally on every cold boot (line 1015), and `sys_reboot(COLD)`
on nRF52840 NCS is actually an NVIC reset — the SoftDevice Controller's
connection table can retain ghost entries across warm resets. Only a full
power cycle clears it.

**Fix:** Add two guards to the inner loop:

1. **Idle-connection timeout** (60 s) — if connected but OtaManager stays
   IDLE and no BEGIN is pending, force-disconnect.
2. **Hard session ceiling** (300 s) — no single OTA transfer should exceed
   5 min regardless of state.

```cpp
const uint32_t connect_start = k_uptime_get_32();
uint32_t connected_idle_ticks = 0;
while (ota_connected) {
    // ... existing deferred BEGIN + commit ...

    // Hard ceiling
    if ((k_uptime_get_32() - connect_start) > 300000u) {
        printk("ota: max session time exceeded, disconnecting\n");
        if (ota_conn) bt_conn_disconnect(ota_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        break;
    }

    // Idle-connection guard
    if (ota_manager.state() == helix::OtaState::IDLE && !ota_begin_pending) {
        if (++connected_idle_ticks >= 1200) {   // 60 s
            printk("ota: connection idle 60s, disconnecting\n");
            if (ota_conn) bt_conn_disconnect(ota_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
            break;
        }
    } else {
        connected_idle_ticks = 0;
    }

    // ... existing stall detection + LED + sleep ...
}
```

**Impact:** HIGH — Tag can be permanently stuck in BLE mode, never resuming
ESB mocap streaming. Only a power cycle recovers it.

---

## ACTION ITEMS SUMMARY

| # | Severity | File | Finding | Action |
|---|----------|------|---------|--------|
| 1 | 🔴 **HIGH** | `main.cpp:917–947` | Phantom BLE connection hangs Tag forever | Add idle timeout (60s) + hard session ceiling (300s) |
| 2 | 🔴 Medium | `main.cpp:187–191` | Race in `helix_request_ota_trigger` | Gate with `target_node=0` + DMB before arming |
| 3 | 🔴 Low | `main.cpp:505` | Comment says sizeof is 9, actually 10 | Fix comment, add named constant |
| 4 | 🟡 Low | `ota_hub_relay.cpp:27–40` | Dead `data_tx_sem` + callback | Delete dead code |
| 5 | 🟡 Medium | `main.cpp:869` | `'V'` marker byte is a debug leftover | Revert to `'H'` or document intentional change |
| 6 | 🟡 Medium | `hub_ota_upload.py:110` | Default target-id changed `70→71` | Verify fleet consistency |
| 7 | 🟡 Low | `main.cpp:702` | Hardcoded offsets for target_id parse | Use memcpy + static_assert |
| 8 | 🟡 Low | `main.cpp:795` | Extra blank line | Remove |
| 9 | 🟢 Nice | `main.cpp:148` | Raw 0xFE000 address | Add BUILD_ASSERT against PM symbol |

---

*End of review.*
