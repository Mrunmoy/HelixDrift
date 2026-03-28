#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BASE_BRANCH="${1:-$(git -C "$ROOT_DIR" branch --show-current)}"
WORKTREE_ROOT="${ROOT_DIR}/.worktrees"

mkdir -p "$WORKTREE_ROOT"

create_one() {
    local team_name="$1"
    local branch_name="$2"
    local target_dir="${WORKTREE_ROOT}/${team_name}"

    if [[ -d "$target_dir" ]]; then
        echo "skip: ${target_dir} already exists"
        return
    fi

    echo "creating ${team_name} worktree from ${BASE_BRANCH}"
    git -C "$ROOT_DIR" worktree add "$target_dir" -b "$branch_name" "$BASE_BRANCH"
}

create_one "architect"  "agent/architect"
create_one "sensors"    "agent/sensors"
create_one "fusion"     "agent/fusion"
create_one "rf-sync"    "agent/rf-sync"
create_one "nrf52"      "agent/nrf52"
create_one "host-tools" "agent/host-tools"
create_one "pose-inference" "agent/pose-inference"

cat <<EOF

Worktrees created under:
  ${WORKTREE_ROOT}

Suggested next step:
  cd ${WORKTREE_ROOT}/architect
  # start an agent session there

If you want tmux panes as well, create a session and open one pane per worktree.
EOF
