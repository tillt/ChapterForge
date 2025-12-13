#!/usr/bin/env bash
# Generate synthetic chapter JSON profiles for tests.
set -euo pipefail

OUTDIR="testdata"
mkdir -p "${OUTDIR}"

if ls "${OUTDIR}"/chapters*.json >/dev/null 2>&1; then
    echo "[generate_test_json] chapter JSON already present, skipping generation."
    exit 0
fi

cat > "${OUTDIR}/chapters.json" <<'EOF'
{
  "title": "ChapterForge Sample (Five)",
  "artist": "DJ Unexpected",
  "album": "Uneven Pads",
  "genre": "Test Audio",
  "year": "2025",
  "comment": "Five bumpy jumps with synth pads and bad jokes.",
  "cover": "cover.jpg",
  "chapters": [
    { "start_ms": 0,     "title": "Wobble Warmup", "url": "https://chapterforge.test/wobble", "image": "chapter1.jpg" },
    { "start_ms": 1200,  "title": "Left Shoe Disco", "url": "https://chapterforge.test/shoe", "image": "chapter2.jpg" },
    { "start_ms": 3200,  "title": "Snack Break", "url": "https://chapterforge.test/snack", "image": "chapter1.jpg" },
    { "start_ms": 5800,  "title": "Sudden Cliffhanger", "url": "https://chapterforge.test/cliff", "image": "chapter2.jpg" },
    { "start_ms": 8400,  "title": "Okay Bye", "url": "https://chapterforge.test/bye", "image": "chapter1.jpg" }
  ]
}
EOF
echo "Wrote ${OUTDIR}/chapters.json"

cat > "${OUTDIR}/chapters_nometa.json" <<'EOF'
{
  "chapters": [
    { "start_ms": 0, "title": "Silent Hello" },
    { "start_ms": 2200, "title": "Mildly Lost" },
    { "start_ms": 5100, "title": "Found Snacks" },
    { "start_ms": 7600, "title": "Dramatic Pause" },
    { "start_ms": 9100, "title": "End Credits" }
  ]
}
EOF
echo "Wrote ${OUTDIR}/chapters_nometa.json"

# Helper to emit chapter arrays cycling provided image names.
emit_chapters() {
  local count=$1; shift
  local images=("$@")
  local skew=137
  local jitter=750
  local base=5000
  local t=0
  for ((i=1; i<=count; ++i)); do
    local delta=$(( base + ((i*skew) % (2*jitter)) - jitter ))
    if [[ ${delta} -lt 1000 ]]; then delta=1000; fi
    t=$((t + delta))
    local start=$((t - delta))
    local img=${images[$(( (i-1) % ${#images[@]} ))]}
    printf '    { "start_ms": %d, "title": "Chapter %d", "image": "%s", "url": "https://chapterforge.test/ch%d" }' \
      "${start}" "${i}" "${img}" "${i}"
    if [[ $i -lt $count ]]; then
      echo ","
    else
      echo
    fi
  done
}

# Normal profile JSON (5 chapters, 800px images)
{
  cat <<'EOF'
{
  "title": "ChapterForge Longform – Five Tangents",
  "artist": "The Pad Sprites",
  "album": "Extended Neon Stories",
  "genre": "Test Audio",
  "year": "2025",
  "comment": "Five uneven stops on a 60s pad ride.",
  "cover": "normal1.jpg",
  "chapters": [
EOF
  imgs=(normal1.jpg normal2.jpg normal3.jpg normal4.jpg normal5.jpg)
  emit_chapters 5 "${imgs[@]}"
  cat <<'EOF'
  ]
}
EOF
} > "${OUTDIR}/chapters_normal_5.json"
echo "Wrote ${OUTDIR}/chapters_normal_5.json"

# Normal profile without metadata (5 chapters)
{
  cat <<'EOF'
{
  "chapters": [
EOF
  imgs=(normal1.jpg normal2.jpg normal3.jpg normal4.jpg normal5.jpg)
  emit_chapters 5 "${imgs[@]}"
  cat <<'EOF'
  ]
}
EOF
} > "${OUTDIR}/chapters_normal_nometa_5.json"
echo "Wrote ${OUTDIR}/chapters_normal_nometa_5.json"

# Large profile: 50 chapters, cycling 2000px images large1..large50.
{
  cat <<'EOF'
{
  "title": "ChapterForge Large50",
  "artist": "The Pad Sprites",
  "album": "Lots of Motifs",
  "genre": "Test Audio",
  "year": "2025",
  "cover": "large1.jpg",
  "chapters": [
EOF
  imgs=()
  for i in $(seq 1 50); do imgs+=("large${i}.jpg"); done
  emit_chapters 50 "${imgs[@]}"
  cat <<'EOF'
  ]
}
EOF
} > "${OUTDIR}/chapters_large_50.json"
echo "Wrote ${OUTDIR}/chapters_large_50.json"
