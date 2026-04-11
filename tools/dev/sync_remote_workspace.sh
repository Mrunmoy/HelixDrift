#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "usage: $0 <user@host> [remote_repo_dir]" >&2
  echo "example: $0 litu@hpserver1 ~/sandbox/embedded/HelixDrift" >&2
  exit 2
fi

REMOTE="$1"
REMOTE_DIR="${2:-~/sandbox/embedded/HelixDrift}"

ssh "${REMOTE}" "mkdir -p ${REMOTE_DIR@Q}"

exec rsync -az --delete \
  --exclude 'build/' \
  --exclude '.deps/' \
  --exclude '.direnv/' \
  --exclude '.cache/' \
  --exclude '__pycache__/' \
  --exclude '*.pyc' \
  --exclude '.DS_Store' \
  "${REPO_ROOT}/" "${REMOTE}:${REMOTE_DIR}/"
