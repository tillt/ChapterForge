#!/usr/bin/env bash
# Generate clean “neon on dark” poster-style chapter artwork with numbered labels.
# Outputs:
#   - chapter1.jpg .. chapter9.jpg at 1280x720 plus cover.jpg
#   - big1.jpg .. big35.jpg at 800x800
# Requirements: ffmpeg with drawtext (fontconfig) available on PATH.
set -euo pipefail

OUTDIR="testdata"
mkdir -p "${OUTDIR}"

# Soft gradient pairs (top/bottom) and matching accents.
TOP_COLORS=("#0b132b" "#0d1b2a" "#0a0f1f" "#0b1024" "#0c1c2c" "#10192a")
BOT_COLORS=("#1f2a44" "#1f4068" "#1c2e4a" "#2a1f3f" "#17324a" "#22324f")
ACCENTS=("#6fffe9" "#ff7edb" "#7dff8a" "#ffd166" "#9f7bff" "#7be0ff")

make_image() {
    local name=$1 width=$2 height=$3 label=$4 idx=$5

    local top=${TOP_COLORS[$((idx % ${#TOP_COLORS[@]}))]}
    local bot=${BOT_COLORS[$((idx % ${#BOT_COLORS[@]}))]}
    local accent=${ACCENTS[$((idx % ${#ACCENTS[@]}))]}

    local font_size=$((height / 4))
    local brand_size=$((height / 14))
    local grid_gap=$((width / 18))

    # Choose a motif to make each image distinct.
    local motif=$((idx % 4))
    local pattern=""
    if [[ ${motif} -eq 0 ]]; then
        pattern="drawgrid=width=${grid_gap}:height=${grid_gap}:thickness=1:color=${accent}@0.09"
    elif [[ ${motif} -eq 1 ]]; then
        pattern="geq=r='mod((X-${width}/2)*(X-${width}/2)+(Y-${height}/2)*(Y-${height}/2),255)':g='mod((X-${width}/2)*(X-${width}/2),255)':b='mod((Y-${height}/2)*(Y-${height}/2),255)',boxblur=10"
    elif [[ ${motif} -eq 2 ]]; then
        pattern="drawgrid=width=$((grid_gap/2)):height=$((grid_gap/2)):thickness=2:color=${accent}@0.12,drawgrid=width=${grid_gap}:height=${grid_gap}:thickness=1:color=${accent}@0.08"
    else
        pattern="geq=r='128+40*sin(0.04*X+0.01*Y)':g='128+40*sin(0.02*X-0.03*Y)':b='128+40*sin(0.03*X+0.02*Y)'"
    fi

    # Build the filter graph: vertical gradient, blur, motif overlay, bloom/screen blend, glow-ish number, brand tag.
    local filters="[0:v][1:v]blend=all_expr='A*(1-Y/H)+B*(Y/H)',boxblur=6,${pattern},split[bbase][bbright];[bbright]curves=all='0/0 0.4/0.05 0.7/1 1/1',gblur=sigma=12,eq=brightness=0.02:saturation=1.15[glow];[bbase][glow]blend=all_mode=screen:all_opacity=0.55,drawtext=font=Helvetica:text='${label}':fontcolor=${accent}@0.9:bordercolor=${accent}@0.4:borderw=16:fontsize=${font_size}:x=(w-text_w)/2:y=(h-text_h)/2:shadowx=6:shadowy=6:shadowcolor=${accent}@0.25,drawtext=font=Helvetica:text='${label}':fontcolor=${accent}:bordercolor=${accent}@0.15:borderw=4:fontsize=${font_size}:x=(w-text_w)/2:y=(h-text_h)/2:shadowx=2:shadowy=2:shadowcolor=${accent}@0.3,drawtext=font=Helvetica:text='ChapterForge':fontcolor=${accent}@0.9:fontsize=${brand_size}:x=w-30-text_w:y=h-30-text_h:shadowx=2:shadowy=2:shadowcolor=${accent}@0.25"

    ffmpeg -v quiet -y \
        -f lavfi -i "color=c=${top}:s=${width}x${height}" \
        -f lavfi -i "color=c=${bot}:s=${width}x${height}" \
        -filter_complex "${filters}" \
        -frames:v 1 "${OUTDIR}/${name}"
}

echo "Generating 1280x720 chapter images..."
for i in $(seq 1 2); do
    make_image "chapter${i}.jpg" 1280 720 "${i}" $((i-1))
done
make_image "cover.jpg" 1280 720 "Cover" 99

echo "Generating 800x800 big images..."
for i in $(seq 1 12); do
    make_image "big${i}.jpg" 800 800 "${i}" $((i-1))
done

echo "Images written to ${OUTDIR}"
