#!/usr/bin/env bash
# Generate synthetic, copyright-free test audio with “retro space pad” vibes and playful
# voiceovers. Think “Interstellar on a Sega GameCube”.
# Outputs:
#   testdata/input.m4a (10s, two 5s chapters) and input.aac
#   testdata/input_large.m4a (250s, fifty 5s chapters) and input_large.aac
# Requires: macOS `say` and `ffmpeg` on PATH.
set -euo pipefail

OUTDIR="testdata"
STEMS_DIR="${OUTDIR}/stems"
# Skip if outputs already exist.
if ls "${OUTDIR}"/input*.m4a "${OUTDIR}"/input*.aac >/dev/null 2>&1; then
  echo "[generate_test_audio] audio fixtures already present, skipping generation."
  exit 0
fi
if ! command -v say >/dev/null 2>&1; then
  echo "[generate_test_audio] 'say' not found; skipping audio generation."
  exit 0
fi
# Explicit voices to avoid platform defaults (number voice must be male).
# Numbers: layered Whisper + deep male (Bruce/Fred/Alex). Comments: female.
NUMBER_WHISPER_CAND=("Whisper" "Alex")
NUMBER_DEEP_CAND=("Grandpa" "Bruce" "Fred" "Alex")
CB_VOICE_CAND=("Samantha" "Victoria")

# Arpeggiator scale ratios (quasi minor-ish) to sweep harmonies across chapters.
ARP_SCALE=(1.0 1.12246 1.18921 1.33484 1.49830 1.68179 1.88775 2.0)

# Number chains: both voices at the same pace; whisper carries a reverse-style cascaded echo.
WHISPER_CHAIN="atempo=0.85,areverse,aecho=0.7:0.6:650:0.4,aecho=0.6:0.5:2200|3200:0.35|0.25,areverse,volume=3.0[wout]"
# Deep voice: pitched down ~1.5 octaves, slowed more (~3x), dry (no echo).
DEEP_CHAIN="asetrate=44100/3,aresample=44100,atempo=0.9,volume=1.0[dout]"
mkdir -p "${OUTDIR}" "${STEMS_DIR}"
TMP=$(mktemp -d)
trap 'rm -rf "${TMP}"' EXIT

pick_voice() {
  local __outvar=$1; shift
  local choice=""
  for v in "$@"; do
    if say -v "$v" -o "${TMP}/.voicecheck.aiff" "" >/dev/null 2>&1; then
      choice="$v"
      break
    fi
  done
  if [[ -z "$choice" ]]; then
    choice="$1"
  fi
  printf -v "${__outvar}" "%s" "$choice"
}

pick_voice NUMBER_WHISPER "${NUMBER_WHISPER_CAND[@]}"
pick_voice NUMBER_DEEP "${NUMBER_DEEP_CAND[@]}"
pick_voice CB_VOICE "${CB_VOICE_CAND[@]}"

say_seg() {
  local voice=$1 text=$2 out=$3
  # Generate AIFF with a chosen system voice; avoid custom formats for portability across macOS versions.
  say -v "${voice}" -o "${out}" "${text}"
}

ECHO_DELAY_MS=500
ECHO_DECAY=0.45

make_seg() {
  local basefreq=$1 number_whisper_aiff=$2 number_deep_aiff=$3 comment_aiff=$4 out=$5 idx=$6
  local lp_expr=20000
  ffmpeg -y -loglevel error \
    -f lavfi -i "anullsrc=channel_layout=stereo:sample_rate=44100:d=8" \
    -i "${number_whisper_aiff}" \
    -i "${number_deep_aiff}" \
    -i "${comment_aiff}" \
    -filter_complex "\
[0:a]anull[padmix];\
[1:a]${WHISPER_CHAIN};\
[2:a]${DEEP_CHAIN};\
[wout][dout]amix=inputs=2:normalize=0[number];\
[3:a]adelay=2000|2000,highpass=f=1200,lowpass=f=3800,acompressor=threshold=-18dB:ratio=2:attack=5:release=60,volume=0.6[com];\
[padmix]pan=stereo|c0=c0|c1=c0[pad_for_mix];\
[com]pan=stereo|c0=c0|c1=c0[comst];\
[number]asplit=2[nl][nr];\
[nl]adelay=0|0,pan=stereo|c0=c0|c1=0*c0[nl_st];\
[nr]adelay=320|320,pan=stereo|c0=0*c0|c1=c0[nr_st];\
[nl_st][nr_st]amix=inputs=2:normalize=0,alimiter=limit=0.9,afade=t=out:st=0:d=12[number_st];\
[number_st]asplit=2[vox_for_mix][vox_for_out];\
[comst]asplit=2[cb_for_mix][cb_for_out];\
[pad_for_mix][vox_for_mix][cb_for_mix]amix=inputs=3:normalize=1,afade=t=in:ss=0:d=0.25,afade=t=out:st=7.6:d=0.4[mix]" \
    -map "[mix]" -ac 2 "${out}" \
    -map "[vox_for_out]" -ac 2 "${STEMS_DIR}/vox_${idx}.wav" \
    -map "[cb_for_out]" -ac 2 "${STEMS_DIR}/cb_${idx}.wav"
}

# 50 fun voice lines to rotate through (female CB-ish)
VOICE_LINES=(
  "Engage vector drive."
  "Snack warp in three."
  "Coffee thrusters online."
  "Plot twist ahead."
  "Debug the stars."
  "Cache the cosmos."
  "Retro boosters humming."
  "Insert quarter to continue."
  "Warping to chapter."
  "Enable disco shields."
  "Ion cannon muted."
  "Snack bar detected."
  "Friendly glitch incoming."
  "Press start for vibes."
  "Magenta nebula crossing."
  "Scanlines holding steady."
  "CRT glow nominal."
  "Synth whales singing."
  "Low-poly comet dodged."
  "Turbulence? Nah."
  "Sega stars aligning."
  "Jumpgate humming."
  "Fuzzy logic engaged."
  "Echoes of cartridges."
  "Floating through menus."
  "Waveform waltz."
  "Sneaking past boss fight."
  "Bonus level unlocked."
  "Laser harp tuned."
  "Pixel dust everywhere."
  "VHS tracking fixed."
  "Floppy fuel at 99 percent."
  "Hyperlane loading..."
  "Chiptune choir rising."
  "Turbo pads primed."
  "Quantum snack procured."
  "Nebula noodles served."
  "Retro drive shimmering."
  "Night mode: ON."
  "Arcade cabinet warmed."
  "Time stretch pleasant."
  "Remember to save game."
  "Wavefolders smiling."
  "Tape hiss embraced."
  "Stay weird, traveler."
  "Mainframe says hi."
  "Warp donuts acquired."
  "Cooldown commencing."
  "One more level."
  "Glitch in style."
)

# Helper to pick a voice line by index (looping) without announcing the number.
voice_line() {
  local idx=$1
  local base="${VOICE_LINES[$((idx % ${#VOICE_LINES[@]}))]}"
  echo "${base}"
}

# 10s sample (two overlapping 5s-offset chapters)
say_seg "${NUMBER_WHISPER}" "1" "${TMP}/numw1.aiff"
say_seg "${NUMBER_DEEP}" "1" "${TMP}/numd1.aiff"
say_seg "${CB_VOICE}" "$(voice_line 0)" "${TMP}/cb1.aiff"
say_seg "${NUMBER_WHISPER}" "2" "${TMP}/numw2.aiff"
say_seg "${NUMBER_DEEP}" "2" "${TMP}/numd2.aiff"
say_seg "${CB_VOICE}" "$(voice_line 1)" "${TMP}/cb2.aiff"
make_seg 220 "${TMP}/numw1.aiff" "${TMP}/numd1.aiff" "${TMP}/cb1.aiff" "${TMP}/seg1.wav" 0
make_seg 330 "${TMP}/numw2.aiff" "${TMP}/numd2.aiff" "${TMP}/cb2.aiff" "${TMP}/seg2.wav" 1

ffmpeg -y -loglevel error -i "${TMP}/seg1.wav" -i "${TMP}/seg2.wav" -i "${OUTDIR}/images/cover_input_normal.jpg" \
  -filter_complex "[0:a]adelay=0|0[a0];[1:a]adelay=5000|5000[a1];[a0][a1]amix=inputs=2:normalize=0,highpass=f=60,volume=15dB,alimiter=limit=0.95,atrim=duration=10,afade=t=out:st=9.8:d=0.2[mix]" \
  -metadata title="ChapterForge Sample 10s (Pads)" \
  -metadata artist="ChapterForge Bot" \
  -metadata album="Synthetic Pad Chapters" \
  -metadata genre="Test Audio" \
  -metadata comment="Synthetic pads with voiceover chapters" \
  -map "[mix]" -map 2:v -c:a aac -b:a 128k -c:v mjpeg -disposition:v attached_pic "${OUTDIR}/input.m4a"

ffmpeg -y -loglevel error -i "${TMP}/seg1.wav" -i "${TMP}/seg2.wav" \
  -filter_complex "[0:a]adelay=0|0[a0];[1:a]adelay=5000|5000[a1];[a0][a1]amix=inputs=2:normalize=0,highpass=f=60,volume=15dB,alimiter=limit=0.95,atrim=duration=10,afade=t=out:st=9.8:d=0.2[mix]" \
  -metadata title="ChapterForge Sample 10s (Pads)" \
  -metadata artist="ChapterForge Bot" \
  -metadata album="Synthetic Pad Chapters" \
  -metadata genre="Test Audio" \
  -metadata comment="Synthetic pads with voiceover chapters" \
  -map "[mix]" -c:a aac -b:a 128k -f adts "${OUTDIR}/input.aac"

# 250s sample (fifty 5s chapters) for large tests
pad_freqs=(220 240 260 280 300 320 340 360 380 400 230 250 270 290 310 330 350 370 390 410 225 245 265 285 305 325 345 365 385 405 235 255 275 295 315 335 355 375 395 415 210 230 250 270 290 310 330 350 370 390)
inputs=()
for i in $(seq 1 50); do
  say_seg "${NUMBER_WHISPER}" "${i}" "${TMP}/numw${i}.aiff"
  say_seg "${NUMBER_DEEP}" "${i}" "${TMP}/numd${i}.aiff"
  say_seg "${CB_VOICE}" "$(voice_line $((i-1)))" "${TMP}/cb${i}.aiff"
  freq=${pad_freqs[$(( (i-1) % ${#pad_freqs[@]} ))]}
  make_seg "${freq}" "${TMP}/numw${i}.aiff" "${TMP}/numd${i}.aiff" "${TMP}/cb${i}.aiff" "${TMP}/seg${i}.wav" $((i-1))
  inputs+=(-i "${TMP}/seg${i}.wav")
done

mix_filter=""
mix_inputs=""
for idx in $(seq 0 49); do
  delay=$(( idx * 5000 ))
  mix_filter+="[${idx}:a]adelay=${delay}|${delay}[d${idx}];"
  mix_inputs+="[d${idx}]"
done
mix_filter+="${mix_inputs}amix=inputs=50:normalize=0,highpass=f=60,volume=15dB,alimiter=limit=0.95,atrim=duration=250,afade=t=out:st=248:d=2[mix]"

ffmpeg -y -loglevel error "${inputs[@]}" -i "${OUTDIR}/images/cover_input_large.jpg" \
  -filter_complex "${mix_filter}" \
  -metadata title="ChapterForge Galactic Tour (Pads)" \
  -metadata artist="Sega Nebulae" \
  -metadata album="Interstellar Cartridge Deluxe" \
  -metadata genre="Chiptune Padwave" \
  -metadata comment="250s of faux-8bit cosmic pads with dramatic whispers" \
  -metadata composer="Lagrange Point Crew" \
  -metadata publisher="ChapterForge Labs" \
  -metadata encoder="ChapterForge RetroSynth" \
  -map "[mix]" -map 50:v -c:a aac -b:a 128k -c:v mjpeg -disposition:v attached_pic "${OUTDIR}/input_large.m4a"

ffmpeg -y -loglevel error "${inputs[@]}" \
  -filter_complex "${mix_filter}" \
  -metadata title="ChapterForge Galactic Tour (Pads)" \
  -metadata artist="Sega Nebulae" \
  -metadata album="Interstellar Cartridge Deluxe" \
  -metadata genre="Chiptune Padwave" \
  -metadata comment="250s of faux-8bit cosmic pads with dramatic whispers" \
  -metadata composer="Lagrange Point Crew" \
  -metadata publisher="ChapterForge Labs" \
  -metadata encoder="ChapterForge RetroSynth" \
  -map "[mix]" -c:a aac -b:a 128k -f adts "${OUTDIR}/input_large.aac"

# Preview: numbers 1-5 with dual voices (whisper + deep) only (no pads) for quick listening.
for i in $(seq 1 5); do
  ffmpeg -y -loglevel error \
    -i "${TMP}/numw${i}.aiff" -i "${TMP}/numd${i}.aiff" \
    -filter_complex "[0:a]${WHISPER_CHAIN};[1:a]${DEEP_CHAIN};[wout][dout]amix=inputs=2:normalize=0" \
    -ac 1 "${TMP}/numharm${i}.wav"
done

ffmpeg -y -loglevel error \
  -i "${TMP}/numharm1.wav" -i "${TMP}/numharm2.wav" -i "${TMP}/numharm3.wav" \
  -i "${TMP}/numharm4.wav" -i "${TMP}/numharm5.wav" \
  -filter_complex "[0:a][1:a][2:a][3:a][4:a]concat=n=5:v=0:a=1,asetpts=PTS-STARTPTS" \
  -c:a aac -b:a 128k "${OUTDIR}/number_harmony_preview.m4a"

echo "Generated pad-based test audio in ${OUTDIR} (with metadata, no embedded chapters)."
