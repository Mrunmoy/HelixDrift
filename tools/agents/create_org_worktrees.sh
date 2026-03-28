#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BASE_BRANCH="${1:-$(git -C "$ROOT_DIR" branch --show-current)}"
WORKTREE_ROOT="${ROOT_DIR}/.worktrees"

mkdir -p "$WORKTREE_ROOT"

create_one() {
    local name="$1"
    local branch_name="$2"
    local target_dir="${WORKTREE_ROOT}/${name}"

    if [[ -d "$target_dir" ]]; then
        echo "skip: ${target_dir} already exists"
        return
    fi

    echo "creating ${name} from ${BASE_BRANCH}"
    git -C "$ROOT_DIR" worktree add "$target_dir" -b "$branch_name" "$BASE_BRANCH"
}

# Codex org
create_one "codex-lead"         "agent/codex-lead"
create_one "codex-sensors"      "agent/codex-sensors"
create_one "codex-fusion"       "agent/codex-fusion"
create_one "codex-host-tools"   "agent/codex-host-tools"
create_one "codex-nrf52"        "agent/codex-nrf52"
create_one "codex-integration"  "agent/codex-integration"

# Claude org
create_one "claude-lead"         "agent/claude-lead"
create_one "claude-systems"      "agent/claude-systems"
create_one "claude-pose"         "agent/claude-pose"
create_one "claude-review"       "agent/claude-review"
create_one "claude-integration"  "agent/claude-integration"

# Kimi org
create_one "kimi-lead"          "agent/kimi-lead"
create_one "kimi-rf-sync"       "agent/kimi-rf-sync"
create_one "kimi-pose"          "agent/kimi-pose"
create_one "kimi-hardware"      "agent/kimi-hardware"
create_one "kimi-review"        "agent/kimi-review"
create_one "kimi-integration"   "agent/kimi-integration"

cat <<EOF

Org worktrees created under:
  ${WORKTREE_ROOT}

Recommended usage:
- org leads coordinate subteams and keep .agents/orgs/*/ORG_STATUS.md updated
- subteams work only within assigned ownership scopes
- org integration worktrees consolidate subteam changes before top-level merge
EOF
