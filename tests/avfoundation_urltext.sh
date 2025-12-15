#!/usr/bin/env bash
# AVFoundation test: ensure URL track tx3g samples contain url_text payloads.
# Usage: AVF_INPUT=path/to/input.m4a AVF_JSON=path/to/chapters.json AVF_OUT=path/to/output.m4a ./tests/avfoundation_urltext.sh

set -euo pipefail

FILE_IN="${AVF_INPUT:-}"
FILE_JSON="${AVF_JSON:-}"
FILE_OUT="${AVF_OUT:-}"
BIN="${AVF_BIN:-./chapterforge_cli}"
TMPDIR_SAFE="${TMPDIR:-/tmp}"
export SWIFT_MODULE_CACHE_PATH="${SWIFT_MODULE_CACHE_PATH:-$TMPDIR_SAFE/swift_module_cache}"
export CLANG_MODULE_CACHE_PATH="${CLANG_MODULE_CACHE_PATH:-$TMPDIR_SAFE/clang_module_cache}"
export XDG_CACHE_HOME="${XDG_CACHE_HOME:-$TMPDIR_SAFE}"
export OBJC_DISABLE_INITIALIZE_FORK_SAFETY=YES
mkdir -p "$SWIFT_MODULE_CACHE_PATH" "$CLANG_MODULE_CACHE_PATH"

if [ -z "$FILE_IN" ] || [ -z "$FILE_JSON" ] || [ -z "$FILE_OUT" ]; then
  echo "AVF_INPUT, AVF_JSON, AVF_OUT env vars must be set" >&2
  exit 1
fi
if [ ! -f "$FILE_IN" ]; then
  echo "input not found: $FILE_IN" >&2; exit 1
fi
if [ ! -f "$FILE_JSON" ]; then
  echo "json not found: $FILE_JSON" >&2; exit 1
fi

if [ ! -x "$BIN" ]; then
  echo "chapterforge_cli not found/executable at: $BIN" >&2
  exit 1
fi

"$BIN" "$FILE_IN" "$FILE_JSON" "$FILE_OUT"

SWIFT_SRC="$(mktemp "$TMPDIR_SAFE/avf_urltext.XXXXXX.swift")"
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
let textTracks = asset.tracks(withMediaType: .text)
if textTracks.isEmpty {
    fputs("no text tracks found\n", stderr)
    exit(1)
}

var foundOne = false
var foundTwo = false

for t in textTracks {
    guard let reader = try? AVAssetReader(asset: asset) else { continue }
    let output = AVAssetReaderTrackOutput(track: t, outputSettings: nil)
    reader.add(output)
    if !reader.startReading() { continue }
    while let sb = output.copyNextSampleBuffer() {
        if let bb = CMSampleBufferGetDataBuffer(sb) {
            let len = CMBlockBufferGetDataLength(bb)
            var data = Data(count: len)
            data.withUnsafeMutableBytes { (ptr: UnsafeMutableRawBufferPointer) in
                _ = CMBlockBufferCopyDataBytes(bb, atOffset: 0, dataLength: len, destination: ptr.baseAddress!)
            }
            if data.contains("custom-url-text-one".data(using: .utf8)!) { foundOne = true }
            if data.contains("custom-url-text-two".data(using: .utf8)!) { foundTwo = true }
        }
    }
    if foundOne && foundTwo { break }
}

if !foundOne || !foundTwo {
    fputs("URL text payloads not found via AVFoundation: one=\(foundOne) two=\(foundTwo)\n", stderr)
    exit(1)
}

print("AVFoundation URL text OK: found custom-url-text-one/two")
SWIFT

trap 'rm -f "$SWIFT_SRC"' EXIT
AVF_FILE="$FILE_OUT" swift "$SWIFT_SRC"
