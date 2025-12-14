#!/usr/bin/env bash
# Generate CRT-flavored, neon-on-dark artwork with numbered labels.
# Output files land in testdata/images (small/normal/large sets).
set -euo pipefail

OUTDIR="testdata/images"
mkdir -p "${OUTDIR}"

# QuickTime is picky about JPEG chroma: force 4:2:0 (yuvj420p) on all renders.
PIX_FMT="yuvj420p"

# Pick a font that works across macOS, Linux, and Windows runners.
FONT_PATH=""
for candidate in \
    "/Library/Fonts/Arial.ttf" \
    "/System/Library/Fonts/Supplemental/Arial.ttf" \
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf" \
    "/usr/share/fonts/truetype/freefont/FreeSans.ttf" \
    "/c/Windows/Fonts/arial.ttf" \
    "/cygdrive/c/Windows/Fonts/arial.ttf"; do
    if [[ -f "${candidate}" ]]; then
        FONT_PATH="${candidate}"
        break
    fi
done

if [[ -z "${FONT_PATH}" ]]; then
    echo "Warning: No known font found; ffmpeg may fall back to defaults."
    FONT_SPEC="font=Helvetica-Bold"
else
    FONT_SPEC="fontfile='${FONT_PATH}'"
fi

# neon_pop settings from generate_variations.sh
COLOR1="#00f0ff"
COLOR2="#0c1024"
SPACING1=8
SPACING2=6
GHOST_OFFSET=12
SCROLL_STEP=-480
SAT=1.6
CTR=1.15

make_image() {
    local name=$1 width=$2 height=$3 label=$4 idx=$5 label_suffix=$6
    local outfile="${OUTDIR}/${name}"
    if [[ -f "${outfile}" ]]; then
        echo "  skip ${name} (already exists)"
        return
    fi

    # Make cover text loud and clear.
    local label_text
    label_text=$(echo "${label}" | tr '[:lower:]' '[:upper:]')

    local font_size=$((height / 4))
    local brand_size_small=$((width * 16 / 100)) # slightly bolder presence
    local big_scroll=$(( -2 * width + (idx * SCROLL_STEP) ))

    # Scale pattern sizes: 1x small, 2x normal, 5x large.
    local scale=1
    if [[ ${width} -ge 2000 ]]; then
        scale=5
    elif [[ ${width} -ge 800 ]]; then
        scale=2
    fi
    local sp1=$((SPACING1 * scale))
    local sp2=$((SPACING2 * scale))
    # Lower frequencies for larger scale to make waves/lines thicker.
    local f1=$(awk -v s=${scale} 'BEGIN { printf("%.6f", 0.27/s) }')
    local f2=$(awk -v s=${scale} 'BEGIN { printf("%.6f", 0.1125/s) }')
    local f3=$(awk -v s=${scale} 'BEGIN { printf("%.6f", 0.225/s) }')
    local f4=$(awk -v s=${scale} 'BEGIN { printf("%.6f", 0.18/s) }')
    local f5=$(awk -v s=${scale} 'BEGIN { printf("%.6f", 0.315/s) }')
    local f6=$(awk -v s=${scale} 'BEGIN { printf("%.6f", 0.135/s) }')

    # Minimal filter graph that matches neon_pop final composition.
    FILTERS=$(cat <<EOF
[0:v][1:v]blend=all_expr='A*(1-0.6*Y/H)+B*(0.6*Y/H)',gblur=sigma=8,curves=all='0/0 0.35/0.05 0.7/1 1/1',eq=saturation=${SAT}:contrast=${CTR},vignette=PI/4:0.7[base];
color=c=black@0:s=${width}x${height},geq=r='128+110*sin(${f1}*X+${f2}*Y)':g='128+110*sin(${f3}*X-${f4}*Y)':b='128+110*sin(${f5}*X+${f6}*Y)',eq=saturation=2.8:contrast=1.05:brightness=0.06,curves=all='0/0 0.35/0.2 0.65/1 1/1',gblur=sigma=1.0,format=rgba[rainbow];
[rainbow][base]blend=all_mode=addition:all_opacity=0.6[mix];
[2:v]geq=r='if(eq(mod(floor(X/(${sp1}/2)),3),0),255,0)':g='if(eq(mod(floor(X/(${sp1}/2)),3),1),255,0)':b='if(eq(mod(floor(X/(${sp1}/2)),3),2),255,0)':a='if(eq(mod(floor(X/(${sp1}/2)),3),0)+eq(mod(floor(X/(${sp1}/2)),3),1)+eq(mod(floor(X/(${sp1}/2)),3),2),255,0)',format=rgba,eq=saturation=3.5:brightness=0.2,gblur=sigma=1.2[scan1];
[3:v]geq=r='if(eq(mod(floor(X/(${sp2}/2)),3),0),255,0)':g='if(eq(mod(floor(X/(${sp2}/2)),3),1),255,0)':b='if(eq(mod(floor(X/(${sp2}/2)),3),2),255,0)':a='if(eq(mod(floor(X/(${sp2}/2)),3),0)+eq(mod(floor(X/(${sp2}/2)),3),1)+eq(mod(floor(X/(${sp2}/2)),3),2),235,0)',format=rgba,eq=saturation=3.0:brightness=0.15,gblur=sigma=0.9[scan2];
[4:v]drawtext=${FONT_SPEC}:text='ChapterForge':fontcolor=#ffb000@1.0:fontsize=${brand_size_small}:x=(w-text_w)/2:y=(h-text_h)/2+(${height}/4):shadowx=-4:shadowy=-4:shadowcolor=#ff7a00@0.3,format=rgba[brand_small_src];
[4:v]drawtext=${FONT_SPEC}:text='ChapterForge':fontcolor=#ffd60a@0.2:fontsize=$((height*3)):x=${big_scroll}:y=(h-text_h)/2:shadowx=10:shadowy=10:shadowcolor=#ff7a00@0.05,format=rgba[brand_big_src];
[brand_small_src]colorchannelmixer=aa=0.8[brand_small];
[brand_big_src]colorchannelmixer=aa=0.0005[brand_big];
[brand_big][brand_small]blend=all_mode=addition:all_opacity=0.05[brand_pure];
[mix][brand_pure]blend=all_mode=addition:all_opacity=1.0[brand_mix];
[brand_mix][scan1]blend=all_mode=multiply:all_opacity=0.3[s1];
[s1][scan2]blend=all_mode=multiply:all_opacity=0.3[stage_scan];
color=c=black@0:s=${width}x${height},format=rgba[ov_small];
[ov_small]drawtext=${FONT_SPEC}:text='ChapterForge':fontcolor=#ffb000@1.0:fontsize=${brand_size_small}:x=(w-text_w)/2:y=(h-text_h)/2+(${height}/4):shadowx=-4:shadowy=-4:shadowcolor=#ff7a00@0.3[ov_s_ready];
color=c=black@0:s=${width}x${height},format=rgba[ov_big];
[ov_big]drawtext=${FONT_SPEC}:text='ChapterForge':fontcolor=#ffd60a@0.25:fontsize=$((height*3)):x=${big_scroll}:y=(h-text_h)/2:shadowx=8:shadowy=8:shadowcolor=#ff7a00@0.08[ov_b_ready];
[stage_scan][ov_s_ready]overlay=format=auto[tmp1];
[tmp1][ov_b_ready]overlay=format=auto[stage_color];
[4:v]drawtext=${FONT_SPEC}:text='${label_text}':fontcolor=red:fontsize=${font_size}:x=(w-text_w)/2-${GHOST_OFFSET}:y=(h-text_h)/2-${GHOST_OFFSET}:shadowx=2:shadowy=2:shadowcolor=red@0.3,format=rgba[r];
[4:v]drawtext=${FONT_SPEC}:text='${label_text}':fontcolor=green:fontsize=${font_size}:x=(w-text_w)/2+${GHOST_OFFSET}:y=(h-text_h)/2+${GHOST_OFFSET}:shadowx=2:shadowy=2:shadowcolor=green@0.3,format=rgba[g];
[4:v]drawtext=${FONT_SPEC}:text='${label_text}':fontcolor=blue:fontsize=${font_size}:x=(w-text_w)/2+${GHOST_OFFSET}/2:y=(h-text_h)/2-${GHOST_OFFSET}:shadowx=2:shadowy=2:shadowcolor=blue@0.3,format=rgba[b];
[r][g]blend=all_mode=screen[rg];
[rg][b]blend=all_mode=screen[ghost];
[stage_color][ghost]overlay=format=auto:alpha=0.05[final_s];
[final_s]drawtext=${FONT_SPEC}:text='${label_suffix}':fontcolor=#ffd60a@0.9:fontsize=$((height/14)):x=w-tw-24:y=h-th-24:shadowx=2:shadowy=2:shadowcolor=#ff7a00@0.4[final_out]
EOF
    )
    ffmpeg -v error -y \
        -f lavfi -i "color=c=${COLOR1}:s=${width}x${height}" \
        -f lavfi -i "color=c=${COLOR2}:s=${width}x${height}" \
        -f lavfi -i "color=c=black@0:s=${width}x${height},format=rgba" \
        -f lavfi -i "color=c=black@0:s=${width}x${height},format=rgba" \
        -f lavfi -i "color=c=black@0:s=${width}x${height},format=rgba" \
        -filter_complex "${FILTERS}" \
        -pix_fmt ${PIX_FMT} \
        -map "[final_out]" -frames:v 1 "${outfile}"
}

echo "Generating 400x400 small chapter images..."
for i in $(seq 1 5); do
    echo "  small ${i}/5"
    make_image "chapter${i}.jpg" 400 400 "${i}" $((i-1)) ""
done
# Generate distinct cover assets for each profile size (output and input variants).
make_image "cover.jpg" 400 400 "cover" 99 "OUTPUT"
make_image "cover_input.jpg" 400 400 "cover" 199 "INPUT"

echo "Generating 800x800 normal images..."
for i in $(seq 1 12); do
    echo "  normal ${i}/12"
    make_image "normal${i}.jpg" 800 800 "${i}" $((i-1)) ""
done
make_image "cover_normal.jpg" 800 800 "cover" 299 "OUTPUT"
make_image "cover_input_normal.jpg" 800 800 "cover" 399 "INPUT"

echo "Generating 2000x2000 large images..."
for i in $(seq 1 50); do
    printf "  large %02d/50\r" "${i}"
    make_image "large${i}.jpg" 2000 2000 "${i}" $((i-1)) ""
done
printf "\n"
make_image "cover_large.jpg" 2000 2000 "cover" 499 "OUTPUT"
make_image "cover_input_large.jpg" 2000 2000 "cover" 599 "INPUT"

echo "Images written to ${OUTDIR}"
