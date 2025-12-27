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

print("AVF dump for \(path)")
for t in asset.tracks {
    print("track id=\(t.trackID) media=\(t.mediaType.rawValue) language=\(t.languageCode ?? "nil")")
}

let groups = asset.chapterMetadataGroups(bestMatchingPreferredLanguages: Locale.preferredLanguages)
print("chapters=\(groups.count)")
for (idx, group) in groups.enumerated() {
    let tr = group.timeRange
    let start = CMTimeGetSeconds(tr.start)
    let dur = CMTimeGetSeconds(tr.duration)
    print(String(format: "[%02d] start=%.3f dur=%.3f", idx, start, dur))
    for item in group.items {
        let key = item.commonKey?.rawValue ?? (item.key?.description ?? "(no key)")
        let ident = item.identifier?.rawValue ?? "(no ident)"
        var val: String = "(non-string)"
        if let s = item.stringValue {
            val = s
        } else if let d = item.dataValue {
            val = "data len=\(d.count)"
        }
        let extra = item.extraAttributes ?? [:]
        let locale = item.locale?.identifier ?? "nil"
        print("  item key=\(key) ident=\(ident) locale=\(locale) value=\(val) extra=\(extra)")
    }
}

// Dump raw text samples for every text track so the shell harness can grep payloads.
let textTracks = asset.tracks(withMediaType: .text)
for t in textTracks {
    guard let reader = try? AVAssetReader(asset: asset) else { continue }
    let output = AVAssetReaderTrackOutput(track: t, outputSettings: nil)
    reader.add(output)
    if !reader.startReading() { continue }
    while let sb = output.copyNextSampleBuffer() {
        let pts = CMTimeGetSeconds(CMSampleBufferGetPresentationTimeStamp(sb))
        let dur = CMTimeGetSeconds(CMSampleBufferGetDuration(sb))
        if let bb = CMSampleBufferGetDataBuffer(sb) {
            let len = CMBlockBufferGetDataLength(bb)
            var data = Data(count: len)
            data.withUnsafeMutableBytes { (ptr: UnsafeMutableRawBufferPointer) in
                _ = CMBlockBufferCopyDataBytes(bb, atOffset: 0, dataLength: len, destination: ptr.baseAddress!)
            }
            let text = String(data: data, encoding: .utf8) ?? "<non-utf8 len=\(len)>"
            print(String(format: "sample track=%d pts=%.3f dur=%.3f payload=%@", t.trackID, pts, dur, text))
        }
    }
}
SWIFT

trap 'rm -f "$SWIFT_SRC"' EXIT
LOG_OUT=$(AVF_FILE="$FILE_OUT" swift "$SWIFT_SRC")
echo "$LOG_OUT"

PYBIN="${PYTHON_BIN:-python3}"
if ! command -v "$PYBIN" >/dev/null 2>&1; then
  PYBIN=python
fi

# Post-process in Python: compare Swift log against JSON expectations
"$PYBIN" - "$FILE_JSON" "$LOG_OUT" <<'PY'
import json, sys, re
json_path, log = sys.argv[1], sys.argv[2]
with open(json_path, "r", encoding="utf-8") as f:
    j = json.load(f)

chapters = j.get("chapters", [])
logtxt = log

def fail(msg):
    sys.stderr.write(msg + "\n")
    sys.exit(1)

# Validate url_text entries appear with their start PTS in the Swift dump
for idx, c in enumerate(chapters):
    url_text = c.get("url_text", "")
    start_ms = c.get("start_ms", 0)
    if not url_text:
        continue
    pts = start_ms / 1000.0
    pattern = rf"pts={pts:.3f}.*payload=.*{re.escape(url_text)}"
    if not re.search(pattern, logtxt, re.DOTALL):
        fail(f"Missing url_text '{url_text}' at pts~{pts:.3f}s in Swift log")

# Validate hrefs (if any) show up either in sample payload or extraAttributes dump
hrefs = [c.get("url", "") for c in chapters if c.get("url")]
for href in hrefs:
    # Require the href to appear in the chapterMetadataGroups extraAttributes (HREF),
    # not just in raw text payload dumps.
    pattern = rf"item key=.*extra=.*HREF.*{re.escape(href)}"
    if not re.search(pattern, logtxt):
        fail(f"Missing href '{href}' in chapterMetadataGroups (extraAttributes)")

print("AVFoundation URL text OK (strings, timings, hrefs present)")
PY

echo "AVFoundation URL text OK"
