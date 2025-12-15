#!/usr/bin/env bash
# Generate synthetic chapter JSON profiles for tests.
set -euo pipefail

OUTDIR="testdata"
mkdir -p "${OUTDIR}"

if ls "${OUTDIR}"/chapters*.json >/dev/null 2>&1; then
    echo "[generate_test_json] chapter JSON already present, skipping generation."
    exit 0
fi

# Helper to query duration (ms) via ffprobe; fall back to a default if missing.
probe_duration_ms() {
    local file=$1 default_ms=$2
    if command -v ffprobe >/dev/null 2>&1 && [[ -f "${file}" ]]; then
        local dur
        dur=$(ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 "${file}" 2>/dev/null | awk '{printf("%.0f",$1*1000)}')
        if [[ -n "${dur}" && "${dur}" -gt 0 ]]; then
            echo "${dur}"
            return
        fi
    fi
    echo "${default_ms}"
}

INPUT_SMALL="${OUTDIR}/input.m4a"
DUR_SMALL_MS=$(probe_duration_ms "${INPUT_SMALL}" 10000)
CHAPTER_LEN_MS=5000
CH_COUNT_SMALL=$(( DUR_SMALL_MS / CHAPTER_LEN_MS ))
if [[ ${CH_COUNT_SMALL} -lt 1 ]]; then CH_COUNT_SMALL=1; fi

cat > "${OUTDIR}/chapters.json" <<EOF
{
  "title": "ChapterForge Sample (Auto)",
  "artist": "DJ Unexpected",
  "album": "Uneven Pads",
  "genre": "Test Audio",
  "year": "2025",
  "comment": "Auto-sized chapters based on input length.",
  "cover": "images/cover_normal.jpg",
  "chapters": [
EOF
for i in $(seq 1 ${CH_COUNT_SMALL}); do
  start_ms=$(( (i-1) * CHAPTER_LEN_MS ))
  title_list=("Wobble Warmup" "Left Shoe Disco" "Snack Break" "Sudden Cliffhanger" "Okay Bye")
  url_list=("wobble" "shoe" "snack" "cliff" "bye")
  idx=$(( (i-1) % ${#title_list[@]} ))
  title=${title_list[$idx]}
  url=${url_list[$idx]}
  img="images/chapter${i}.jpg"
  printf '    { "start_ms": %d, "title": "%s", "url": "https://chapterforge.test/%s", "image": "%s" }' \
    "${start_ms}" "${title}" "${url}" "${img}" >> "${OUTDIR}/chapters.json"
  if [[ $i -lt ${CH_COUNT_SMALL} ]]; then
    echo "," >> "${OUTDIR}/chapters.json"
  else
    echo >> "${OUTDIR}/chapters.json"
  fi
done
cat >> "${OUTDIR}/chapters.json" <<'EOF'
  ]
}
EOF
echo "Wrote ${OUTDIR}/chapters.json (chapters based on input duration)"

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

# Normal profile JSON (5 chapters, 800px images) sized to 10s input: 0/2/4/6/8s.
cat > "${OUTDIR}/chapters_10s_2ch_normalimg_meta.json" <<'EOF'
{
  "title": "ChapterForge Longform – Five Tangents",
  "artist": "The Pad Sprites",
  "album": "Extended Neon Stories",
  "genre": "Test Audio",
  "year": "2025",
  "comment": "Five quick stops on a 10s clip.",
  "cover": "images/cover_normal.jpg",
  "chapters": [
    { "start_ms": 0,    "title": "Chapter 1", "image": "images/normal1.jpg", "url": "https://chapterforge.test/ch1" },
    { "start_ms": 2000, "title": "Chapter 2", "image": "images/normal2.jpg", "url": "https://chapterforge.test/ch2" },
    { "start_ms": 4000, "title": "Chapter 3", "image": "images/normal3.jpg", "url": "https://chapterforge.test/ch3" },
    { "start_ms": 6000, "title": "Chapter 4", "image": "images/normal4.jpg", "url": "https://chapterforge.test/ch4" },
    { "start_ms": 8000, "title": "Chapter 5", "image": "images/normal5.jpg", "url": "https://chapterforge.test/ch5" }
  ]
}
EOF
echo "Wrote ${OUTDIR}/chapters_10s_2ch_normalimg_meta.json"

# Normal profile without metadata (5 chapters), same timing as above.
cat > "${OUTDIR}/chapters_10s_2ch_normalimg_nometa.json" <<'EOF'
{
  "chapters": [
    { "start_ms": 0,    "title": "Chapter 1", "image": "images/normal1.jpg", "url": "https://chapterforge.test/ch1" },
    { "start_ms": 2000, "title": "Chapter 2", "image": "images/normal2.jpg", "url": "https://chapterforge.test/ch2" },
    { "start_ms": 4000, "title": "Chapter 3", "image": "images/normal3.jpg", "url": "https://chapterforge.test/ch3" },
    { "start_ms": 6000, "title": "Chapter 4", "image": "images/normal4.jpg", "url": "https://chapterforge.test/ch4" },
    { "start_ms": 8000, "title": "Chapter 5", "image": "images/normal5.jpg", "url": "https://chapterforge.test/ch5" }
  ]
}
EOF
echo "Wrote ${OUTDIR}/chapters_10s_2ch_normalimg_nometa.json"

# Offset-first-chapter profile (first chapter intentionally starts after 0ms).
cat > "${OUTDIR}/chapters_10s_offset_first.json" <<'EOF'
{
  "title": "Offset Start Chapters",
  "artist": "Latency Lovers",
  "album": "Delayed Entry",
  "comment": "First chapter does not start at t=0 to exercise warning path.",
  "cover": "images/cover_normal.jpg",
  "chapters": [
    { "start_ms": 1500, "title": "Late Arrival", "image": "images/normal1.jpg", "url": "https://chapterforge.test/late" },
    { "start_ms": 4500, "title": "Settling In", "image": "images/normal2.jpg", "url": "https://chapterforge.test/settle" },
    { "start_ms": 8000, "title": "Exit Soon",   "image": "images/normal3.jpg", "url": "https://chapterforge.test/exit" }
  ]
}
EOF
echo "Wrote ${OUTDIR}/chapters_10s_offset_first.json"

# Large profile: 50 chapters, 5s spacing, fits 250s input.
cat > "${OUTDIR}/chapters_250s_50ch_largeimg_meta.json" <<'EOF'
{
  "title": "ChapterForge Large50",
  "artist": "The Pad Sprites",
  "album": "Lots of Motifs",
  "genre": "Test Audio",
  "year": "2025",
  "cover": "images/cover_large.jpg",
  "chapters": [
EOF
{
  imgs=()
  for i in $(seq 1 50); do imgs+=("images/large${i}.jpg"); done
  start=0
  for i in $(seq 1 50); do
    img=${imgs[$((i-1))]}
    printf '    { "start_ms": %d, "title": "Chapter %d", "image": "%s", "url": "https://chapterforge.test/ch%d" }' \
      "${start}" "${i}" "${img}" "${i}"
    if [[ $i -lt 50 ]]; then
      echo ","
    else
      echo
    fi
    start=$((start + 5000))
  done
}
cat <<'EOF'
  ]
}
EOF
echo "Wrote ${OUTDIR}/chapters_250s_50ch_largeimg_meta.json"
