# v19 Stage 4 deploy findings (2026-04-22 → 2026-04-23 night)

First actual deploy of Stage 4 Path X1 (Tag-side TDMA slot
scheduling). Outcome: **rate collapsed, jitter unchanged.**

## First false deploy (v19 fake = stale v18)

Initial v19 build failed with a brace error in `node_handle_anchor()`,
but `fleet_ota.sh` didn't detect the failure (it only checked
`zephyr.signed.bin` existence). All 10 Tags got flashed with the
previous v18 merged.hex.

Caught when SWD-read of Tag 1 showed no `stage4_*` symbols present.

Fixes:
- `main.cpp`: scoped the Stage-4 demotion-on-miss into the
  `if (!found)` branch (commit `c7697da`).
- `fleet_ota.sh`: pre-deletes `merged.hex` + `zephyr.signed.bin`
  before rebuild; also captures the build's exit code (commit
  `b71be53`).

## Real v19 deploy

All 10 Tags flashed with actual Stage 4 code. Tag 1 slot0 = 0x13
confirmed.

## Capture results (20 min, 120-s settle)

| Tag | frames | rate | mean bias | p99 \|err\| |
|---:|---:|---:|---:|---:|
| 1 | 866 | 0.72 Hz | -11943 µs | 20.5 ms |
| 2 | 729 | 0.61 Hz | -8569 µs | 20.5 ms |
| 3 | 540 | 0.45 Hz | -11236 µs | 19.5 ms |
| 4 | 1309 | 1.09 Hz | -14389 µs | 22.5 ms |
| 5 | 930 | 0.78 Hz | -9896 µs | 21.0 ms |
| 6 | 1022 | 0.85 Hz | -12406 µs | 20.5 ms |
| 7 | 1052 | 0.88 Hz | -12567 µs | 21.0 ms |
| 8 | 1086 | 0.91 Hz | -12854 µs | 21.0 ms |
| 9 | 692 | 0.58 Hz | -9108 µs | 20.0 ms |
| 10 | 1245 | 1.04 Hz | -546860 µs | 31.0 ms |

**TX rate collapsed from 43 Hz → < 1 Hz across the fleet.** Cross-Tag
span analysis couldn't even find bins with ≥ 5 Tags (all Tags too
intermittent).

## Tag 1 Stage 4 RAM state

```
stage4_state_val    = 2 (LOCKED)
stage4_stable_count = 4
stage4_promotions   = 6
stage4_demotions    = 5
stage4_tx_in_slot   = 2433
stage4_tx_skipped   = 141,109   ← 98 % of attempts skipped
```

## Root cause

My first cut of the slot-aligned TX logic in `maybe_send_frame`
**skipped** any frame where `delay_us > SLOT_US`:

```c
} else {
    /* too far — skip this iteration, main loop will retry. */
    stage4_tx_skipped++;
    atomic_set(&tx_ready, 1);
    return;
}
```

Main loop `k_sleep(20 ms)` cadence equals the Stage 4 cycle period
exactly. So the next iteration lands at the **same** `cycle_phase` —
which, if outside the slot, skips again, forever.

## Fix (landed in v20, 2026-04-23)

Never skip; always `k_usleep(delay_us)` to reach the next slot start
(0–cycle µs), then TX. Combined period: `main_sleep_20ms + delay_us ≈
20–40 ms` → 25–50 Hz. Sub-optimal throughput but correct behaviour.

If v20 still has rate issues, next iteration will move the slot
alignment onto a `k_timer` or shorten the main loop sleep when
LOCKED.

## Positive: midpoint locks ARE acquired

`stage4_promotions = 6` shows Stage 3 midpoint did reach sub-ms
stability six separate times. The lock tracker works. The bug is
purely in the TX scheduling path.

## Takeaways for RF.md

- Stage 4 Path X1 lock-acquisition logic is sound; only the TX
  scheduling was broken in v19.
- fleet_ota.sh hardened to detect build failures going forward.
- v19 sync-error numbers are not useful — too few frames per Tag.
- Cross-Tag span measurement pending v20 redeploy.
