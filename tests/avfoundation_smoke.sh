#!/usr/bin/env bash
# Simple AVFoundation smoke test: load the M4A via a small Swift runner and dump chapter count.
# Usage: AVF_OUTPUT=path/to/file ./tests/avfoundation_smoke.sh

set -euo pipefail

FILE="${AVF_OUTPUT:-}"
TMPDIR_SAFE="${TMPDIR:-/tmp}"
export SWIFT_MODULE_CACHE_PATH="${SWIFT_MODULE_CACHE_PATH:-$TMPDIR_SAFE/swift_module_cache}"
export CLANG_MODULE_CACHE_PATH="${CLANG_MODULE_CACHE_PATH:-$TMPDIR_SAFE/clang_module_cache}"
export XDG_CACHE_HOME="${XDG_CACHE_HOME:-$TMPDIR_SAFE}"
export OBJC_DISABLE_INITIALIZE_FORK_SAFETY=YES
mkdir -p "$SWIFT_MODULE_CACHE_PATH"
mkdir -p "$CLANG_MODULE_CACHE_PATH"
if [ -z "$FILE" ]; then
  echo "AVF_OUTPUT env var must point to the M4A to check" >&2
  exit 1
fi
if [ ! -f "$FILE" ]; then
  echo "File not found: $FILE" >&2
  exit 1
fi

SWIFT_SRC="$(mktemp "$TMPDIR_SAFE/avf_smoke.XXXXXX.swift")"
cat >"$SWIFT_SRC" <<'SWIFT'
import AVFoundation
import Foundation

let path = ProcessInfo.processInfo.environment["AVF_FILE"] ?? ""
if path.isEmpty {
    fputs("AVF_FILE not set\n", stderr)
    exit(1)
}
let url = URL(fileURLWithPath: path)
let asset = AVURLAsset(url: url)
let chapters = asset.chapterMetadataGroups(withTitleLocale: Locale.current, containingItemsWithCommonKeys: [])
print("chapters=\(chapters.count)")
for (idx, group) in chapters.enumerated() {
    let titleItem = group.items.first(where: { $0.commonKey?.rawValue == "title" })
    let title = titleItem?.stringValue ?? "(nil)"
    let timeRange = group.timeRange
    let start = CMTimeGetSeconds(timeRange.start)
    let dur = CMTimeGetSeconds(timeRange.duration)
    print(String(format: "[%02d] %.3fs dur=%.3fs title=%@", idx, start, dur, title))
}
SWIFT

trap 'rm -f "$SWIFT_SRC"' EXIT

echo "Running AVFoundation smoke test on $FILE"
AVF_FILE="$FILE" swift "$SWIFT_SRC"
