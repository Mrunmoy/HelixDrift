● Search (grep)
  │ "ack_lat|last_anchor_queue_us|ESB_EVENT_TX_SUCCESS|central_handle_frame" in
  │ **/*.{c,cpp,h,hpp}
  └ 13 lines found

I found the exact instrumentation points; I’m reading those now so the call on bias and next step is grounded in the actual queue/TX behavior.

● Read main.cpp
  │ zephyr_apps/nrf52840-mocap-bridge/src/main.cpp
  └ L200:230 (31 lines read)

● Read main.cpp
  │ zephyr_apps/nrf52840-mocap-bridge/src/main.cpp
  └ L500:525 (26 lines read)

● Read main.cpp
  │ zephyr_apps/nrf52840-mocap-bridge/src/main.cpp
  └ L640:670 (31 lines read)

● Read main.cpp
  │ zephyr_apps/nrf52840-mocap-bridge/src/main.cpp
  └ L336:347 (12 lines read)

**Call:** **Q1 no**, **Q2 yes—fix it first with Option A**, **Q3 rx_node_id still stands, but as a correctness hardening step, not a panic response to a 20 ms-dominant pathology.**

**Q1.** The `40/60/0/0` histogram is **not strong enough to prove** the 20 ms slow path is rare. Your bias is asymmetric: it preferentially compresses or drops exactly the cases you most care about—the older queued anchors that sat in FIFO behind newer RXs. So the **absence of 10–30 ms / >=30 ms counts is not conclusive**. What it *does* still suggest is that the system is probably **not dominated by a huge long-tail catastrophe**, because if full-period waits were extremely common you’d usually see other symptoms too; but this measurement alone should be treated as **qualitative only**.

**Q2.** **Fix the instrumentation before Stage 2.** Pick **Option A**. A tiny ring buffer matched to ESB ACK FIFO depth is the right tool because the question is fundamentally **queue-to-TX latency per anchor**, not just pressure. The ISR cost/state is trivial for the value gained, and it gives you a defensible Stage 1 result. **Option B** is useful as a secondary metric, but it cannot answer the cross-contamination timing question by itself. **Option C** leaves too much ambiguity right where you need confidence.

**Q3.** The `2–10 ms` result, if it survives corrected instrumentation, **does reduce severity** versus the earlier “wait a full ~20 ms period” story. Cross-contamination over a few ms is materially less bad than over a full frame period. But it does **not** remove the need for `rx_node_id`: the problem is still a **wrong-anchor-to-tag association**, and even a smaller timing skew can corrupt attribution under close TX spacing. So I would **keep Stage 2 as planned**, but update the rationale: it’s less about rescuing a system in widespread 20 ms failure, and more about **making attribution unambiguous and robust under realistic overlap/jitter**.


Changes   +0 -0
Requests  1 Premium (45s)
Tokens    ↑ 69.2k • ↓ 1.9k • 64.9k (cached) • 1.1k (reasoning)
