#!/usr/bin/env bash
# Generate synthetic, copyright-free test audio with pad-like tones and voiceovers.
# Outputs:
#   testdata/input.m4a (10s, two 5s chapters) and input.aac
#   testdata/input_big.m4a (60s, twelve 5s chapters) and input_big.aac
# Requires: macOS `say` and `ffmpeg` on PATH.
set -euo pipefail

OUTDIR="testdata"
mkdir -p "${OUTDIR}"
TMP=$(mktemp -d)
trap 'rm -rf "${TMP}"' EXIT

say_seg() {
  local text=$1 out=$2
  # Generate AIFF with system voice; avoid custom formats for portability across macOS versions.
  say -v "Samantha" -o "${out}" "${text}"
}

# Build a pad from three detuned sines (robust across ffmpeg builds).
make_seg() {
  local basefreq=$1 speech=$2 out=$3
  ffmpeg -y -loglevel error \
    -f lavfi -i "sine=frequency=${basefreq}:duration=5" \
    -f lavfi -i "sine=frequency=${basefreq}*1.01:duration=5" \
    -f lavfi -i "sine=frequency=${basefreq}*0.99:duration=5" \
    -i "${speech}" \
    -filter_complex "[0:a][1:a][2:a]amix=inputs=3:normalize=0[pad];[pad][3:a]amix=inputs=2:normalize=0,atrim=duration=5" \
    -ac 1 "${out}"
}

# 10s sample (two 5s chapters)
say_seg "Chapter one." "${TMP}/ch1.aiff"
say_seg "Chapter two." "${TMP}/ch2.aiff"
make_seg 220 "${TMP}/ch1.aiff" "${TMP}/seg1.wav"
make_seg 330 "${TMP}/ch2.aiff" "${TMP}/seg2.wav"

concat2="[0:a][1:a]concat=n=2:v=0:a=1,asetpts=PTS-STARTPTS"
ffmpeg -y -loglevel error -i "${TMP}/seg1.wav" -i "${TMP}/seg2.wav" \
  -filter_complex "${concat2}" \
  -metadata title="ChapterForge Sample 10s (Pads)" \
  -metadata artist="ChapterForge Bot" \
  -metadata album="Synthetic Pad Chapters" \
  -metadata genre="Test Audio" \
  -metadata comment="Synthetic pads with voiceover chapters" \
  -c:a aac -b:a 128k "${OUTDIR}/input.m4a"

ffmpeg -y -loglevel error -i "${TMP}/seg1.wav" -i "${TMP}/seg2.wav" \
  -filter_complex "${concat2}" \
  -metadata title="ChapterForge Sample 10s (Pads)" \
  -metadata artist="ChapterForge Bot" \
  -metadata album="Synthetic Pad Chapters" \
  -metadata genre="Test Audio" \
  -metadata comment="Synthetic pads with voiceover chapters" \
  -c:a aac -b:a 128k -f adts "${OUTDIR}/input.aac"

# 60s sample (twelve 5s chapters)
pad_freqs=(220 260 300 240 280 320 260 300 340 230 270 310)
inputs=()
for i in $(seq 1 12); do
  say_seg "Chapter ${i}." "${TMP}/ch${i}.aiff"
  make_seg "${pad_freqs[$((i-1))]}" "${TMP}/ch${i}.aiff" "${TMP}/seg${i}.wav"
  inputs+=(-i "${TMP}/seg${i}.wav")
done

concat12=""
for idx in $(seq 0 11); do concat12+="[${idx}:a]"; done
concat12+="concat=n=12:v=0:a=1,asetpts=PTS-STARTPTS"

ffmpeg -y -loglevel error "${inputs[@]}" \
  -filter_complex "${concat12}" \
  -metadata title="ChapterForge Sample 60s (Pads)" \
  -metadata artist="ChapterForge Bot" \
  -metadata album="Synthetic Pad Chapters Long" \
  -metadata genre="Test Audio" \
  -metadata comment="60s synthetic pad voiceover chapters" \
  -c:a aac -b:a 128k "${OUTDIR}/input_big.m4a"

ffmpeg -y -loglevel error "${inputs[@]}" \
  -filter_complex "${concat12}" \
  -metadata title="ChapterForge Sample 60s (Pads)" \
  -metadata artist="ChapterForge Bot" \
  -metadata album="Synthetic Pad Chapters Long" \
  -metadata genre="Test Audio" \
  -metadata comment="60s synthetic pad voiceover chapters" \
  -c:a aac -b:a 128k -f adts "${OUTDIR}/input_big.aac"

echo "Generated pad-based test audio in ${OUTDIR} (with metadata, no embedded chapters)."
