#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PAGES_DIR="$ROOT_DIR/pages"
PORT="${1:-4173}"
RELOAD_FILE="$PAGES_DIR/.preview-reload"

if ! command -v python3 >/dev/null 2>&1; then
  echo "error: python3 is required for local pages preview" >&2
  exit 1
fi

if [[ ! -d "$PAGES_DIR" ]]; then
  echo "error: pages directory not found: $PAGES_DIR" >&2
  exit 1
fi

echo "Serving bruvtext pages from: $PAGES_DIR"
echo "Open: http://127.0.0.1:${PORT}/"
echo "Press Ctrl+C to stop."

touch "$RELOAD_FILE"
date +%s > "$RELOAD_FILE"

watch_for_changes() {
  local last_stamp=""
  while true; do
    local current_stamp
    current_stamp="$(
      find "$PAGES_DIR" -type f \
        ! -name '.preview-reload' \
        -printf '%T@ %p\n' | sort | tail -n 1
    )"

    if [[ "$current_stamp" != "$last_stamp" ]]; then
      date +%s > "$RELOAD_FILE"
      last_stamp="$current_stamp"
    fi

    sleep 1
  done
}

cleanup() {
  if [[ -n "${WATCH_PID:-}" ]]; then
    kill "$WATCH_PID" >/dev/null 2>&1 || true
  fi
}

trap cleanup EXIT INT TERM

watch_for_changes &
WATCH_PID=$!

cd "$PAGES_DIR"
python3 -m http.server "$PORT" --bind 127.0.0.1
