● Read ci.yml
  │ .github/workflows/ci.yml
  └ L1:260 (176 lines read)

● Read test_ota_manager.cpp
  │ tests/test_ota_manager.cpp
  └ L1:260 (260 lines read)

● Read main.cpp
  │ zephyr_apps/nrf52840-mocap-bridge/src/main.cpp
  └ L1060:1145 (86 lines read)

● Show reviewed commit diff (shell)
  │ git --no-pager show --stat --oneline --decorate=short 5b5058e && printf
  │ '\n---DIFF---\n' && git --no-pager show --format=medium --unified=80 5b5058e
  │ -- .github/workflows/ci.yml tests/test_ota_manager.cpp
  │ zephyr_apps/nrf52840-mocap-bridge/src/main.cpp
  └ 10 lines...

Quick sanity check: I’m verifying the CI proof mechanism against the actual `.gitmodules` layout, because the only remaining risk here would be a mismatch between “iterate by path” and “lookup by submodule name.”

● Read .gitmodules
  └ L1:200 (7 lines read)

● Inspect gitmodules mapping (shell)
  │ git --no-pager show 5b5058e:.github/workflows/ci.yml | sed -n '20,130p' &&
  │ printf '\n--- .gitmodules parsed ---\n' && git config -f .gitmodules
  │ --get-regexp '^submodule\..*\.(path|url)$'
  └ 117 lines...

I found one thing worth sanity-checking in the OTA test: whether `bytesReceived()==0` is actually the same state variable that drives the next expected offset. If it is, the strengthened assertion is sufficient; if not, there’d still be a tiny blind spot.

● Read OtaManager.cpp
  │ firmware/common/ota/OtaManager.cpp
  └ L1:220 (89 lines read)

● Read OtaManager.hpp
  │ firmware/common/ota/OtaManager.hpp
  └ L1:220 (86 lines read)

● Search (grep)
  │ "bytesReceived\(|writeChunk\(|expected offset|bytes_received_|next
  │ offset|offset !=" in **/OtaManager*
  └ 44 lines found

I’ve confirmed the OTA test is now actually probing the real offset-tracking state. I’m doing one last live Git check against the remotes so the sign-off on `fetch <sha>` isn’t just theoretical.

✗ Test fetch-by-sha against submodule remotes (shell)
  │ set -euo pipefail
  │ sf_sha=$(git ls-tree HEAD external/SensorFusion | awk '{print $3}')
  │ mc_sha=$(git ls-tree HEAD third_party/mcuboot | awk '{print $3}')
  │ printf 'SensorFusion sha: %s\n' "$sf_sha"
  │ printf 'mcuboot sha: %s\n' "$mc_sha"…
  └ Permission denied and could not request permission from user

The environment blocked a live `git fetch` probe, so I’m grounding the last answer on Git protocol semantics rather than a runtime experiment. That still lets me answer the review question cleanly.

✗ Fetching web content https://git-scm.com/docs/git-config#Documentation/git-config.txt-uploadpacka…
  └ Permission denied and could not request permission from user

✗ Fetching web content https://git-scm.com/docs/git-fetch · url: "https://git-scm.com/docs/git-fetc…
  └ Permission denied and could not request permission from user

**CLEAN - no further concerns**

**A.** Yes — for this CI on GitHub-hosted remotes, `git fetch --depth=1 "$url" "$sha"` in a fresh temp repo is the right practical proof. It is materially better than `ls-remote`: `ls-remote` only sees advertised ref tips, while `fetch <sha>` checks whether the exact commit is actually fetchable by the upload-pack path that `git submodule update` ultimately depends on. So it correctly covers non-tip ancestor commits that would have been false negatives before.

The only caveat is scope: this proves **fetchability on that server**, not an abstract graph-theoretic notion of “reachable” independent of server policy. A host that disables reachable-SHA wants could reject a perfectly reachable commit and give a false negative. That is not a concern for your current GitHub-based setup, and in practice this is the behavior you care about anyway: “will CI be able to fetch the submodule commit?” Also, your current `.gitmodules` names match their paths, so the `submodule.$path.url` lookup is valid as written.

**B.** No remaining concerns in the reviewed round-7→8 delta. The OTA test now exercises the real in-flight-state reset, and the added `GUARD_US < SLOT_US` assert closes the config hole I was worried about.


Changes   +0 -0
Requests  1 Premium (1m 10s)
Tokens    ↑ 191.6k • ↓ 3.7k • 177.3k (cached) • 2.1k (reasoning)
