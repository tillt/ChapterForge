#!/usr/bin/env bash
# Generate synthetic, copyright-free test audio with “retro space pad” vibes and playful
# voiceovers. Think “Interstellar on a Sega GameCube”.
# Outputs:
#   testdata/input.m4a (10s, two 5s chapters) and input.aac
#   testdata/input_large.m4a (250s, fifty 5s chapters) and input_large.aac
# Requires: macOS `say` and `ffmpeg` on PATH.
set -euo pipefail

OUTDIR="testdata"
# Explicit voices to avoid platform defaults (number voice must be male).
# Numbers: layered Whisper + deep male (Bruce/Fred/Alex). Comments: female.
NUMBER_WHISPER_CAND=("Whisper" "Alex")
NUMBER_DEEP_CAND=("Bruce" "Fred" "Alex")
CB_VOICE_CAND=("Samantha" "Victoria")

# Arpeggiator scale ratios (quasi minor-ish) to sweep harmonies across chapters.
ARP_SCALE=(1.0 1.12246 1.18921 1.33484 1.49830 1.68179 1.88775 2.0)

# Number chains: both voices at the same pace; whisper carries its own spacey echo.
WHISPER_CHAIN="atempo=0.85,aecho=0.6:0.6:500:0.45,volume=3.0[wout]"
DEEP_CHAIN="atempo=0.85,volume=0.6[dout]"
mkdir -p "${OUTDIR}"
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

# Build a pad: detuned sines + Shepard-ish layered octaves + filtered noise + light echo.
# Effects vary gently per segment to keep things quirky without harsh jumps.
VIB_DEPTH=(0.25 0.30 0.35 0.40 0.45 0.50)
LOWPASS=(900 980 1060 1140 1220 1300)
NOISE_VOL=(0.05 0.06 0.07 0.08 0.09 0.10)
CHORUS_MOD=(42 48 54 60 66 72)
# Voice/lead echo tuned longer (~500ms) to meet request.
ECHO_DELAY_MS=500
ECHO_DECAY=0.45

make_seg() {
  local basefreq=$1 number_whisper_aiff=$2 number_deep_aiff=$3 comment_aiff=$4 out=$5 idx=$6
  local vib=${VIB_DEPTH[$((idx % ${#VIB_DEPTH[@]}))]}
  local lp=${LOWPASS[$((idx % ${#LOWPASS[@]}))]}
  local nv=${NOISE_VOL[$((idx % ${#NOISE_VOL[@]}))]}
  local chor=${CHORUS_MOD[$((idx % ${#CHORUS_MOD[@]}))]}
  local arp=${ARP_SCALE[$((idx % ${#ARP_SCALE[@]}))]}
  ffmpeg -y -loglevel error \
    -f lavfi -i "sine=frequency=${basefreq}:duration=8" \
    -f lavfi -i "sine=frequency=${basefreq}*0.5:duration=8" \
    -f lavfi -i "sine=frequency=${basefreq}*2:duration=8" \
    -f lavfi -i "sine=frequency=${basefreq}*4:duration=8" \
    -f lavfi -i "sine=frequency=${basefreq}*1.005:duration=8" \
    -f lavfi -i "sine=frequency=${basefreq}*0.25:duration=8" \
    -f lavfi -i "sine=frequency=${basefreq}*${arp}:duration=8" \
    -f lavfi -i "sine=frequency=${basefreq}*${arp}*2:duration=8" \
    -f lavfi -i "anoisesrc=d=8:c=pink" \
    -i "${number_whisper_aiff}" \
    -i "${number_deep_aiff}" \
    -i "${comment_aiff}" \
    -f lavfi -i "anoisesrc=d=8:c=brown" \
    -filter_complex "\
[0:a]volume=0.9[a0];\
[1:a]volume=0.7[a1];\
[2:a]volume=0.8[a2];\
[3:a]volume=0.5[a3];\
[4:a]volume=0.9[a4];\
[5:a]volume=0.6[a5];\
[6:a]volume=0.7[a6];\
[7:a]volume=0.5[a7];\
[a0][a1][a2][a3][a4][a5][a6][a7]amix=inputs=8:normalize=0[tone];\
[tone]asplit=2[t_det][t_main];\
[t_det]asetrate=44100*1.01,aresample=44100,afade=t=out:st=0:d=4[detune];\
[t_main]afade=t=in:ss=0:d=4[steady];\
[detune][steady]amix=inputs=2:normalize=0[tone_hybrid];\
[tone_hybrid]lowpass=f=${lp},highpass=f=120,asetrate=44100*0.995,aresample=44100,chorus=0.5:0.7:${chor}:0.3:0.18:1.8,flanger=delay=0.4:depth=0.9:regen=0:width=6:speed=0.18[pad];\
[8:a]lowpass=f=820,highpass=f=180,volume=${nv},tremolo=f=4:d=0.4[tex];\
[pad][tex]amix=inputs=2:normalize=0,acompressor=threshold=-14dB:ratio=2.2:attack=10:release=80,alimiter=limit=0.8[padmix];\
[9:a]${WHISPER_CHAIN};\
[10:a]${DEEP_CHAIN};\
[wout][dout]amix=inputs=2:normalize=0,aecho=0.6:0.6:900|1400|1900|2400|2900:0.6|0.5|0.4|0.3|0.2[number];\
[11:a]highpass=f=1200,lowpass=f=3800,acompressor=threshold=-18dB:ratio=2:attack=5:release=60,volume=0.6[comcore];\
[12:a]highpass=f=1000,lowpass=f=3200,volume=0.03,tremolo=f=6:d=0.7[comnoise];\
[comcore][comnoise]amix=inputs=2:normalize=0[com];\
[padmix]pan=stereo|c0=c0|c1=c0[padst];\
[com]pan=stereo|c0=c0|c1=c0[comst];\
[number]asplit=2[nl][nr];\
[nl]adelay=0|0,pan=stereo|c0=c0|c1=0*c0[nl_st];\
[nr]adelay=320|320,pan=stereo|c0=0*c0|c1=c0[nr_st];\
[nl_st][nr_st]amix=inputs=2:normalize=0,alimiter=limit=0.9,afade=t=out:st=0:d=12[number_st];\
[padst][number_st][comst]amix=inputs=3:normalize=0,afade=t=in:ss=0:d=0.25,afade=t=out:st=6.5:d=1.5" \
    -ac 2 "${out}"
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

# Helper to pick a voice line by index (looping) and prepend chapter number.
voice_line() {
  local idx=$1
  local num=$((idx + 1))
  local base="${VOICE_LINES[$((idx % ${#VOICE_LINES[@]}))]}"
  echo "Chapter ${num}. ${base}"
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

ffmpeg -y -loglevel error -i "${TMP}/seg1.wav" -i "${TMP}/seg2.wav" \
  -filter_complex "[0:a]adelay=0|0[a0];[1:a]adelay=5000|5000[a1];[a0][a1]amix=inputs=2:normalize=0,alimiter=limit=0.95" \
  -metadata title="ChapterForge Sample 10s (Pads)" \
  -metadata artist="ChapterForge Bot" \
  -metadata album="Synthetic Pad Chapters" \
  -metadata genre="Test Audio" \
  -metadata comment="Synthetic pads with voiceover chapters" \
  -c:a aac -b:a 128k "${OUTDIR}/input.m4a"

ffmpeg -y -loglevel error -i "${TMP}/seg1.wav" -i "${TMP}/seg2.wav" \
  -filter_complex "[0:a]adelay=0|0[a0];[1:a]adelay=5000|5000[a1];[a0][a1]amix=inputs=2:normalize=0,alimiter=limit=0.95" \
  -metadata title="ChapterForge Sample 10s (Pads)" \
  -metadata artist="ChapterForge Bot" \
  -metadata album="Synthetic Pad Chapters" \
  -metadata genre="Test Audio" \
  -metadata comment="Synthetic pads with voiceover chapters" \
  -c:a aac -b:a 128k -f adts "${OUTDIR}/input.aac"

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
mix_filter+="${mix_inputs}amix=inputs=50:normalize=0,alimiter=limit=0.95"

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
  -c:a aac -b:a 128k "${OUTDIR}/input_large.m4a"

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
  -c:a aac -b:a 128k -f adts "${OUTDIR}/input_large.aac"

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
