I’m checking the current RF/anchor implementation and pulling in a design-review pass so the recommendation is grounded in the actual code path, not just the prior discussion.

● Search (grep)
  │ "esb_.*pipe|rx_pipe|pipe_id|ack_payload|TX_SUCCESS|tx_attempts" in
  │ **/*.{c,cpp,h,hpp,conf,overlay,md} (zephyr_apps)
  └ 15 lines found

● Search (grep)
  │ "anchor_tx_us|central_timestamp_us|rx_node_id|sync-anchor|midpoint|ACK|ack
  │ payload|esb_write_payload|pipe 0|pipe_id|node_id" in
  │ **/*.{c,cc,cpp,h,hpp,md,txt}
  └ 8 lines found

**REQUEST_CHANGES** — `anchor v4` as framed in the brief is incomplete on shared pipe 0.  
- Evidence: Hub builds ACK payloads on `payload->pipe` and all Tags currently share one pipe (`CONFIG_HELIX_MOCAP_PIPE`, default 0), so ACK association is only per-pipe today, not per-Tag. `zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:420-449, 669-673`; `zephyr_apps/nrf52840-mocap-bridge/Kconfig:29-33`; `docs/RF_PROTOCOL_REFERENCE.md:251-253, 208`; `docs/RF_NEXT_STEPS_DESIGN_BRIEF.md:95-109`; `docs/RF_ROBUSTNESS_REPORT.md:158-191`
- Call: if you pursue timestamped anchors, **also add an association field** (`rx_node_id`, ideally echoed RX node/seq) so a Tag can reject an anchor not meant for it.
- Why it matters: otherwise `anchor_tx_us` can still be delivered in the wrong Tag’s ACK on shared pipe 0.

**APPROVE** — Option **1 (`rx_node_id` on anchor, keep shared pipe 0)** is the best **listed** stopgap.  
- Evidence: Tag currently accepts any valid anchor and immediately overwrites `estimated_offset_us`; there is no per-Tag association check. `zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:516-558`
- Fix suggestion: add `rx_node_id` and make Tag ignore mismatches before updating offset.
- Likely impact on `<10 ms` cross-tag p99: **low**. It fixes **wrong-anchor application**, but **does not remove** the dominant ~20 ms slow-path/TIFS bias. Expect correctness to improve, but p99 likely stays roughly in the **30–50 ms** class, not <10 ms.

**REQUEST_CHANGES** — Option **2 (per-Tag pipes)** is not feasible as stated for the current 10-Tag product.  
- Evidence: ESB exposes only **8 pipes**; product target is **10 Tags**. `docs/RF_PROTOCOL_REFERENCE.md:208, 251-253`; `docs/RF_NEXT_STEPS_DESIGN_BRIEF.md:21-24`; `zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:599-645`
- Also omitted: current firmware has a **single compile-time pipe** knob, so per-Tag pipes need either per-Tag images or new flash provisioning/runtime mapping. `zephyr_apps/nrf52840-mocap-bridge/Kconfig:29-33`; `zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:669-673`; `docs/NRF_HUB_RELAY_OTA.md:19-21, 126-149`
- Likely impact on `<10 ms` p99: **low-to-medium even if feasible**, because unique pipes prevent cross-Tag FIFO mixups but **do not remove slow-path anchor latency**.

**REQUEST_CHANGES** — Option **3 (per-Tag pipes + `rx_node_id`)** is over-complex for the gain.  
- Evidence: same 8-pipe limit and provisioning gap as above; wire-format bumps are acceptable now, but per-Tag pipes still require Hub+Tag rollout plus provisioning changes. `docs/NRF_HUB_RELAY_OTA.md:94-111, 167-180`; `zephyr_apps/nrf52840-mocap-bridge/Kconfig:29-33`
- Likely impact on `<10 ms` p99: **still not enough by itself**. It is the safest of the three on correctness, but it still leaves the slow-path latency term intact.

**COMMENT** — The docs disagree on ACK timing, and that should be cleaned up before implementation.  
- `docs/RF_PROTOCOL_REFERENCE.md:525-533` says ACK payload rides on the **next** transmission on that pipe.  
- `docs/RF_NEXT_STEPS_DESIGN_BRIEF.md:79-82` and `docs/RF_ROBUSTNESS_REPORT.md:160-167` assume a **fast-path same-ACK** vs **slow-path next-ACK** split.  
- Fix suggestion: update the protocol reference to match the behavior the design is actually relying on.

## Ranked recommendation
1. **Option 1** — best of the three, but only as a **correctness hardening** step.  
2. **Option 3** — technically safer than 2, but too much rollout/provisioning cost for too little timing benefit.  
3. **Option 2** — not feasible as stated for 10 Tags / 8 ESB pipes.

## Closeout call
**Do not treat 1/2/3 as the `<10 ms` solution.**  
If you need a decision now: **skip per-Tag pipes**, and either:
- ship **Option 1** as a short-term guardrail, **or better**
- go straight to the missing option below.

## A) Missing better option?
**Yes:** add **`rx_node_id` + `anchor_tx_us`** (shared pipe 0 retained).  
That is the smallest change that addresses **both**:
- wrong-Tag anchor consumption on shared pipe 0, and
- the dominant slow-path bias that keeps p99 above 10 ms.

I would **not** recommend:
- **“fast-path only / `central_timestamp_us=0` when uncertain”** as the main fix — the Hub cannot robustly know uncertainty on shared pipe 0, and dropping slow-path anchors trades one ambiguity for stale/no sync.
- **TDMA slot grants** as the next move — feasible long-term, but much larger protocol scope than the current problem requires.

## B) Minimum no-wire-format instrumentation to land first
1. **Tag-side anchor-age histogram**: record `local_anchor_rx_us - last_tx_start_us` bucketed as `<2 ms`, `2–10 ms`, `10–30 ms`, `>30 ms`. This directly tells you how often anchors are fast-path vs one-period-late.  
   - Relevant code points: `maybe_send_frame()` and `node_handle_anchor()`. `zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:660-683, 495-558`
2. **Hub-side per-node RX→enqueue stats**: count per-node frames received, `esb_write_payload()` success/fail, and software timestamp delta from RX ISR entry to enqueue call.  
   - Relevant code: `central_handle_frame()`. `zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:383-449`
3. **Expose counters over existing status/USB text**, not a new wire format yet.  
   - Relevant code: `HelixMocapStatus`, `event_handler()`. `zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:95-113, 562-588`

Net: **Among 1/2/3 choose #1, but the real recommendation is `rx_node_id + anchor_tx_us` on shared pipe 0.****Closeout recommendation**

### Ranking
1. **APPROVE — Approach 1: add `rx_node_id`, keep shared pipe 0**
2. **COMMENT — Approach 3: per-Tag pipes + `rx_node_id`**
3. **REQUEST_CHANGES — Approach 2: split Tags across pipes only**

### Call
**Ship Approach 1 only as the immediate safety fix. Do not spend time on per-Tag pipes for the current 10-Tag system.**  
If you actually need **<10 ms cross-tag p99**, none of 1/2/3 is the real answer; go straight to a wire bump that adds **`rx_node_id` + echoed frame sequence + `anchor_tx_us`** on shared pipe 0.

---

## Per approach

### 1) APPROVE — `rx_node_id` on anchor, shared pipe 0
**Why**
- Current Tag code accepts any valid anchor and immediately overwrites offset; there is no per-Tag association check now (`main.cpp:516-558`).
- This project already uses the same pattern successfully for OTA flood filtering via `ota_target_node_id` on shared pipe 0 (`main.cpp:60-67`, `main.cpp:535-549`, `docs/NRF_HUB_RELAY_OTA.md:83-92`).

**What it fixes**
- Prevents Tag B from applying an anchor that was generated from Tag A’s frame.

**What it does *not* fix**
- It does **not** remove the slow-path/TIFS bias in the current estimator (`main.cpp:555`, `docs/RF_ROBUSTNESS_REPORT.md:169-191`).

**Likely impact on `<10 ms` p99**
- **Low.** Good correctness hardening, but p99 likely stays well above 10 ms.

**Risk**
- Low; wire bump is cheap now, and rollout is acceptable.

---

### 2) COMMENT — per-Tag pipes + `rx_node_id`
**Why it ranks above #2 but still not recommended now**
- Technically safer than pipe-split alone because `rx_node_id` still protects against residual/config mistakes.

**Main problem**
- ESB gives you **8 pipes**, but product target is **10 Tags** (`docs/RF_PROTOCOL_REFERENCE.md:208, 251-253`; `docs/RF_NEXT_STEPS_DESIGN_BRIEF.md:21-30`; `Kconfig:29-33`).
- Also requires Hub + Tag changes plus provisioning/runtime mapping.

**Likely impact on `<10 ms` p99**
- **Still low by itself.** Even with unique pipes, current `central_timestamp_us` estimator still carries slow-path latency.

---

### 3) REQUEST_CHANGES — split Tags across pipes only
**Why last**
- For 10 Tags, this is not fully feasible on one PRX.
- If multiple Tags still share any pipe, the same FIFO-association problem remains.
- Even if fully feasible, it still would not fix the estimator bias.

**Likely impact on `<10 ms` p99**
- **Not enough.**

---

## A) Missing better option?
**Yes. Best practical option:**

**Shared pipe 0 + wire bump adding**
- `rx_node_id`
- **echoed source frame sequence**
- `anchor_tx_us`

**Tag behavior**
- Keep a small ring of recent local TX timestamps keyed by frame sequence.
- Apply midpoint correction **only** if `rx_node_id == my_node_id` and sequence matches a cached TX.
- Otherwise ignore the anchor.

**Why this is the real fix**
- `rx_node_id` fixes wrong-Tag delivery.
- `frame sequence` tells the Tag which local TX timestamp to pair with delayed anchors.
- `anchor_tx_us` removes the slow-path bias.
- This matches your constraints: wire changes are cheap now, and software TX timestamp jitter is acceptable.

**Not recommended first**
- `central_timestamp_us=0` on “uncertain” cases: Hub cannot reliably know that on shared pipe 0.
- TDMA slot grants: too much protocol scope for the current problem.

---

## B) Minimum no-wire-format instrumentation first
Land these **before** big protocol work if you want one data pass:

1. **Tag-side anchor-age histogram**  
   Bucket `anchor_rx_us - last_tx_us` into `<2 ms`, `2-10`, `10-30`, `>30`.  
   (`main.cpp:660-683`, `main.cpp:495-558`)

2. **Tag-side offset-step histogram**  
   Count `abs(new_offset_us - old_offset_us)` buckets. Big jumps will expose bad anchors/stale anchors.

3. **Hub-side RX→enqueue / RX→TX_SUCCESS timing stats**  
   Timestamp at frame RX, after `esb_write_payload()`, and in `ESB_EVENT_TX_SUCCESS`.  
   (`main.cpp:383-449`, `main.cpp:562-588`)

Expose them over existing USB/status; no RF wire change needed.

---

## Bottom line
- **Choose Approach 1 now** as the closeout recommendation.
- **Do not choose per-Tag pipes** for the current 10-Tag / 1-Hub product.
- If `<10 ms` p99 is a real requirement, **skip straight to**:  
  **`rx_node_id` + frame sequence + `anchor_tx_us` on shared pipe 0.**● Design-reviewer Reviewing RF options
  │ Read main.cpp
  │   │ zephyr_apps/nrf52840-mocap-bridge/src/main.cpp
  │   └ L383:449 (67 lines read)
  └ Read RF_PROTOCOL_REFERENCE.md
      │ docs/RF_PROTOCOL_REFERENCE.md
      └ L525:537 (13 lines read)

I found the current code path that accepts any anchor on the shared ACK path, which lines up with the earlier FIFO concern. I’m reading just the relevant spots now so the recommendation can stay sharp and specific.

● Read RF_TIME_SYNC_REFERENCE.md
  │ docs/RF_TIME_SYNC_REFERENCE.md
  └ L1:220 (220 lines read)

● Read RF_ROBUSTNESS_REPORT.md
  │ docs/RF_ROBUSTNESS_REPORT.md
  └ L150:230 (81 lines read)

● Read main.cpp
  │ zephyr_apps/nrf52840-mocap-bridge/src/main.cpp
  └ L500:590 (91 lines read)

● Read RF_PROTOCOL_REFERENCE.md
  │ docs/RF_PROTOCOL_REFERENCE.md
  └ L200:270 (71 lines read)

● Read copilot-tool-output-1776805665296-upjjcg.txt
  │ /tmp/copilot-tool-output-1776805665296-upjjcg.txt
  └ L1:120 (120 lines read)

● Read main.cpp
  │ zephyr_apps/nrf52840-mocap-bridge/src/main.cpp
  └ L360:460 (101 lines read)

**Call:** pick **Approach 1 now** as the safe closeout fix, and **do not promote per-pipe routing to critical path**. It fixes the physical misassociation bug with minimal risk. But it is **not** the long-term `<10 ms p99` answer; for that, the real next protocol rev should be **shared pipe 0 + `rx_node_id` + echoed frame sequence + `anchor_tx_us`**.

**Ranking**
1. **Approach 1 — `rx_node_id` in anchor**
2. **Approach 3 — both**
3. **Approach 2 — per-Tag pipes**

**Why this ranking**
- **Approach 1** directly fixes the known correctness failure: today the Tag accepts any valid anchor and immediately updates offset, while all Tags share pipe 0 and the ACK payload queue is per-pipe. Adding `rx_node_id` makes the Tag ignore anchors not meant for it, exactly like the current OTA flood filtering already does with `ota_target_node_id`. It is a small wire bump, cheap OTA rollout, and preserves the proven 10-Tag / 436 Hz / +16% baseline.
- **Approach 3** is safer than Approach 2 because `rx_node_id` still catches config mistakes during rollout, but it adds provisioning and topology churn for little immediate gain.
- **Approach 2** is the wrong trade now. You only get 8 ESB pipes for a 10-Tag fleet, so some sharing remains unless you redesign the whole topology. More importantly, pipe-splitting fixes **association**, not the **slow-path timing bias** from missing TIFS. So it adds risk without getting you to the fast-motion goal.

**Impact on `<10 ms` cross-Tag p99**
- **Approach 1:** fixes wrong-Tag anchors, but **does not** remove the ~20 ms slow-path estimator bias. Expect correctness to improve, but not `<10 ms p99`.
- **Approach 2:** same story. Even with clean per-pipe association, midpoint math still breaks unless the Tag knows which TX the delayed anchor belongs to.
- **Approach 3:** also not enough by itself.

**Missing better option**
- Yes: **shared pipe 0, but add three fields in the next real sync rev**:
  1. `rx_node_id`
  2. **echoed source frame sequence**
  3. `anchor_tx_us`
  
  Then the Tag can pair a delayed anchor with the correct local TX timestamp and run midpoint math safely. That is the clean path to `<10 ms p99` without pipe-topology churn. TDMA/slot grants are overkill for this sprint.

**Minimum instrumentation to land first, no wire change**
1. **Hub counter/histogram:** `rx_timestamp_us -> esb_write_payload()` latency, plus `rx_timestamp_us -> TX_SUCCESS callback` latency. This gives a direct fast-path/slow-path proxy.
2. **Tag histogram:** `anchor_rx_us - last_local_tx_us`. You will see two lobes if anchors are often arriving on the next frame.
3. **Tag offset-step histogram:** `abs(new_offset_us - old_offset_us)` to quantify bad/stale anchor damage.

**Bottom line:** **Ship Approach 1 now** to make sync physically correct per Tag. Then plan a proper **v5 anchor** on shared pipe 0 with `rx_node_id + frame sequence + anchor_tx_us`; that is the path that can actually hit the fast-motion target.


Changes   +0 -0
Requests  1 Premium (7m 27s)
Tokens    ↑ 799.9k • ↓ 22.3k • 688.0k (cached) • 14.1k (reasoning)
