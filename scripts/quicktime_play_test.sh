#!/usr/bin/env bash
# Manual helper to open an M4A in QuickTime Player and capture its logs.
# Usage: ./scripts/quicktime_play_test.sh [path/to/output.m4a]
# Env vars:
#   DURATION=<seconds to wait before stopping log capture> (default: 10)
#   QT_LOG=<path for the captured log> (default: /tmp/qtplay_<timestamp>.log)

set -euo pipefail

# Default to a build/test_outputs artifact to avoid polluting testdata.
FILE="${1:-build/test_outputs/output.m4a}"
DURATION="${DURATION:-10}"
LOG_FILE="${QT_LOG:-/tmp/qtplay_$(date +%s).log}"

if ! command -v log >/dev/null 2>&1; then
  echo "macOS 'log' command not found; this helper requires macOS." >&2
  exit 1
fi

if [ ! -f "$FILE" ]; then
  echo "File not found: $FILE" >&2
  exit 1
fi

echo "Starting QuickTime Player log capture to: $LOG_FILE"
log stream --style compact --predicate 'process == "QuickTime Player"' >"$LOG_FILE" 2>&1 &
LOG_PID=$!
sleep 1
if ! kill -0 "$LOG_PID" >/dev/null 2>&1; then
  echo "Failed to start log stream; see $LOG_FILE" >&2
  exit 1
fi

echo "Opening file in QuickTime Player: $FILE"
open -a "QuickTime Player" "$FILE"
echo "Waiting ${DURATION}s while QuickTime loads/plays..."
sleep "$DURATION"

echo "Stopping log capture..."
kill "$LOG_PID" >/dev/null 2>&1 || true

echo "Log written to: $LOG_FILE"
echo "You can inspect it with: less \"$LOG_FILE\""
