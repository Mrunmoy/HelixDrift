# Path X1 sanity check — Codex + Copilot (2026-04-22 night)

Question: is Stage 4 Path X1 (no separate broadcast beacon; Tag-side
TDMA scheduling on top of Stage 3 midpoint) architecturally sound?

Context: `docs/RF_STAGE4_EXPLORATORY.md`. Original RF.md §9 proposed
a Hub broadcast beacon; ESB hardware PRX/PTX mode-switching cost
made it non-viable overnight.

## Codex

> **Path X1 is architecturally sound.** A separate broadcast beacon
> mostly gives you faster/common cold-start, explicit cycle epoch,
> fleet slot-map updates, and relock while a Tag is otherwise
> silent. It does not give a fundamentally better timebase than a
> correctly paired Stage 3 midpoint; once TDMA is active, matched
> anchors should become frequent and cleaner.
>
> The main thing not to miss: don't jump from a noisy/free-run
> Stage 3 estimate straight into 2 ms slots. Require a stable
> lock, add guard/hysteresis, and fall back on lock loss. If
> Stage 3 cannot reliably produce <500 us stability before TDMA,
> use a staged acquisition mode: wider slots or sparse single-Tag
> admission before tightening to 2 ms.
>
> I would drop the Hub PTX beacon for v1. Given PRX/PTX mode
> switching wipes ACK FIFO and creates blind windows, it
> undermines the exact queue/collision behavior Stage 4 is meant
> to fix. Keep broadcast beacons as a future feature only if you
> need dynamic slot assignment, fast fleet join, or independent
> maintenance sync.

## Copilot

> **Path X1 is architecturally sound** for v1 TDMA. If the goal
> is "give each Tag a usable Hub-time reference without breaking
> PRX," matched anchors already do that: `anchor_tx_us` is the
> time signal, and once TDMA is active each Tag should get a
> clean per-cycle anchor anyway.
>
> What broadcast beacons would add is mostly **bootstrap and
> resilience**, not steady-state capability:
>
> 1. Uniform cold-start / hub-reboot reacquire for all Tags from
>    one shared epoch.
> 2. Passive sync keepalive even for a Tag that temporarily isn't
>    getting matched anchors.
> 3. Slightly cleaner mental model for a global slot epoch.
>
> Those are real benefits, but not fundamental ones. X1 covers
> the core requirement if you keep the safeguards you described:
> free-run until stable lock, only use foreign anchors for coarse
> bootstrap, and fall back to Stage 3 on lock loss.
>
> So the main thing you'd be missing is **robustness margin**,
> not essential functionality. Given the ESB PRX/PTX switching
> penalty, X1 looks like the right architecture unless you later
> need faster reacquire or passive downlink-only synchronization.

## Consensus

Both: **Path X1 is the right choice for v1.** Both call out:
- Require stable midpoint lock before transitioning to TDMA slots.
- Hysteresis on lock loss / fall back to Stage 3.
- Broadcast beacons are deferrable — only needed for bootstrap
  resilience / fast reacquire, not for the sub-ms timebase.

Proceeding with Path X1 after v18 data analysis.
