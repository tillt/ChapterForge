#!/usr/bin/env bash
# Generate a handful of CRT/Trinitron-styled sample images with layered outputs.
# Variations are written to testdata/variations/<variant>/<idx>/{base,scan1,scan2,brand_big,brand_small,label,rgbghost,final}.png
set -euo pipefail

OUT_ROOT="testdata/variations"
mkdir -p "${OUT_ROOT}"

# name color1 color2 spacing1 spacing2 scan_mode saturation contrast ghost_offset scroll_step
variants=(
  "vivid_strong  #ff7a00 #10192a 6 4 screen 1.5 1.12 10 -14"
  "vivid_soft    #ff3ebb #0b0f1f 7 5 screen 1.4 1.08 8  -12"
  "neon_pop      #00f0ff #0c1024 8 6 add    1.6 1.15 12 -16"
)

render_variant() {
    local name=$1 c1=$2 c2=$3 spacing1=$4 spacing2=$5 scan_mode=$6 sat=$7 ctr=$8 ghost=$9 scroll_step=${10}
    # Normalize blend name
    if [[ "${scan_mode}" == "add" ]]; then
        scan_mode="addition"
    fi
    local outdir="${OUT_ROOT}/${name}"
    mkdir -p "${outdir}"

    for idx in 0 1 2; do
        local label=$((idx + 1))
        local dir="${outdir}/${idx}"
        mkdir -p "${dir}"

        local w=400 h=400
        local font_size=$((h / 4))
        local brand_small=$((w * 14 / 100))
        local big_scroll=$(( -2 * w + (idx * scroll_step) ))

        ffmpeg -v error -y \
          -f lavfi -i "color=c=${c1}:s=${w}x${h}" \
          -f lavfi -i "color=c=${c2}:s=${w}x${h}" \
          -f lavfi -i "color=c=black@0:s=${w}x${h},format=rgba" \
          -f lavfi -i "color=c=black@0:s=${w}x${h},format=rgba" \
          -f lavfi -i "color=c=black@0:s=${w}x${h},format=rgba" \
          -filter_complex "\
            [0:v][1:v]blend=all_expr='A*(1-0.6*Y/H)+B*(0.6*Y/H)',gblur=sigma=8,curves=all='0/0 0.35/0.05 0.7/1 1/1',eq=saturation=${sat}:contrast=${ctr},vignette=PI/4:0.7[btmp];\
            [btmp]split=3[base_s][base_a][base_m];\
            [base_s]split=2[base_out][base_use];\
            color=c=black@0:s=${w}x${h},geq=r='128+110*sin(0.27*X+0.1125*Y)':g='128+110*sin(0.225*X-0.18*Y)':b='128+110*sin(0.315*X+0.135*Y)',eq=saturation=2.8:contrast=1.05:brightness=0.06,curves=all='0/0 0.35/0.2 0.65/1 1/1',gblur=sigma=1.0,format=rgba[rainbow_tmp];\
            [rainbow_tmp]split=2[rainbow_show][rainbow_masked_src];\
            [rainbow_masked_src]split=2[rainbow_masked][rainbow_masked_use];\
            [rainbow_masked_use][base_use]blend=all_mode=addition:all_opacity=0.6[rainbow_use2];\
            [rainbow_use2]split=2[rainbow_for_map][rainbow_for_stack];\
            [2:v]geq=r='if(eq(mod(floor(X/(${spacing1}/2)),3),0),255,0)':g='if(eq(mod(floor(X/(${spacing1}/2)),3),1),255,0)':b='if(eq(mod(floor(X/(${spacing1}/2)),3),2),255,0)':a='if(eq(mod(floor(X/(${spacing1}/2)),3),0)+eq(mod(floor(X/(${spacing1}/2)),3),1)+eq(mod(floor(X/(${spacing1}/2)),3),2),255,0)',format=rgba,eq=saturation=3.5:brightness=0.2,gblur=sigma=1.2[s1tmp];\
            [s1tmp]split=3[scan1_s][scan1_a][scan1_m];\
            [scan1_s]split=2[scan1_out][scan1_use];\
            [3:v]geq=r='if(eq(mod(floor(X/(${spacing2}/2)),3),0),255,0)':g='if(eq(mod(floor(X/(${spacing2}/2)),3),1),255,0)':b='if(eq(mod(floor(X/(${spacing2}/2)),3),2),255,0)':a='if(eq(mod(floor(X/(${spacing2}/2)),3),0)+eq(mod(floor(X/(${spacing2}/2)),3),1)+eq(mod(floor(X/(${spacing2}/2)),3),2),235,0)',format=rgba,eq=saturation=3.0:brightness=0.15,gblur=sigma=0.9[s2tmp];\
            [s2tmp]split=3[scan2_s][scan2_a][scan2_m];\
            [scan2_s]split=2[scan2_out][scan2_use];\
            [4:v]drawtext=font=Helvetica-Bold:text='ChapterForge':fontcolor=#ffb000@1.0:fontsize=${brand_small}:x=(w-text_w)/2:y=(h-text_h)/2+(${h}/4):shadowx=-4:shadowy=-4:shadowcolor=#ff7a00@0.3,format=rgba[stmp];\
            [stmp]split=3[small_s][small_a][small_m];\
            [small_s]split=2[small_out][small_use];\
            [4:v]drawtext=font=Helvetica-Bold:text='ChapterForge':fontcolor=#ffd60a@1.0:fontsize=$((h*3)):x=${big_scroll}:y=(h-text_h)/2:shadowx=10:shadowy=10:shadowcolor=#ff7a00@0.2,format=rgba[bigtmp];\
            [bigtmp]split=3[big_s][big_a][big_m];\
            [big_s]split=2[big_out][big_use];\
            [4:v]drawtext=font=Helvetica-Bold:text='${label}':fontcolor=${c1}@0.95:fontsize=${font_size}:x=(w-text_w)/2:y=(h-text_h)/2:shadowx=4:shadowy=4:shadowcolor=${c2}@0.4,format=rgba[ltmp];\
            [ltmp]split=1[label_out];\
            [4:v]drawtext=font=Helvetica-Bold:text='${label}':fontcolor=red:fontsize=${font_size}:x=(w-text_w)/2-${ghost}:y=(h-text_h)/2-${ghost}:shadowx=2:shadowy=2:shadowcolor=red@0.3,format=rgba[r];\
            [4:v]drawtext=font=Helvetica-Bold:text='${label}':fontcolor=green:fontsize=${font_size}:x=(w-text_w)/2+${ghost}:y=(h-text_h)/2+${ghost}:shadowx=2:shadowy=2:shadowcolor=green@0.3,format=rgba[g];\
            [4:v]drawtext=font=Helvetica-Bold:text='${label}':fontcolor=blue:fontsize=${font_size}:x=(w-text_w)/2+${ghost}/2:y=(h-text_h)/2-${ghost}:shadowx=2:shadowy=2:shadowcolor=blue@0.3,format=rgba[b];\
            [r][g]blend=all_mode=screen[rg];[rg][b]blend=all_mode=screen[ghost_tmp];\
            [ghost_tmp]split=3[ghost_s][ghost_a][ghost_m];\
            [ghost_s]split=2[ghost_out][ghost_use];\
            [big_use][small_use]blend=all_mode=addition:all_opacity=1.0[brand_pure];\
            [rainbow_for_stack][brand_pure]blend=all_mode=addition:all_opacity=1.0[brand1];\
            [brand1]split=2[brand_screen_src][brand_mult];\
            color=c=black@0:s=${w}x${h}[overlay_blank];\
            [overlay_blank]drawtext=font=Helvetica-Bold:text='ChapterForge':fontcolor=#ffb000@1.0:fontsize=${brand_small}:x=(w-text_w)/2:y=(h-text_h)/2+(${h}/4):shadowx=-4:shadowy=-4:shadowcolor=#ff7a00@0.3[brand_overlay_src];\
            color=c=black@0:s=${w}x${h}[overlay_blank_big];\
            [overlay_blank_big]drawtext=font=Helvetica-Bold:text='ChapterForge':fontcolor=#ffd60a@1.0:fontsize=$((h*3)):x=${big_scroll}:y=(h-text_h)/2:shadowx=8:shadowy=8:shadowcolor=#ff7a00@0.25[brand_overlay_big_src];\
            [scan1_use]split=2[scan1_screen][scan1_mult];\
            [scan2_use]split=2[scan2_screen][scan2_mult];\
            [ghost_use]split=2[ghost_screen][ghost_mult];\
            [brand_screen_src][scan1_screen]blend=all_mode=multiply:all_opacity=0.3[s1];\
            [s1][scan2_screen]blend=all_mode=multiply:all_opacity=0.3[stage_scan_tmp];\
            [stage_scan_tmp]split=2[stage_scan_out][stage_scan_for_brand];\
            [brand_overlay_src]colorchannelmixer=aa=0.8[brand_for_overlay];\
            [brand_overlay_big_src]colorchannelmixer=aa=0.05[brand_big_for_overlay];\
            [stage_scan_for_brand][brand_for_overlay]overlay=format=auto[tmp_overlay1];\
            [tmp_overlay1][brand_big_for_overlay]overlay=format=auto[stage_scan_color];\
            [stage_scan_color]split=2[stage_scan_color_out][stage_scan_color_use];\
            [stage_scan_color_use][ghost_screen]overlay=format=auto:alpha=0.05[final_s];\
            [brand_mult][scan1_mult]blend=all_mode=multiply:all_opacity=0.3[ms1];\
            [ms1][scan2_mult]blend=all_mode=multiply:all_opacity=0.3[ms2];\
            [ms2][ghost_mult]overlay=format=auto:alpha=0.05[final_scanmult];\
            [base_a][scan1_a]blend=all_mode=addition[a1];\
            [a1][scan2_a]blend=all_mode=addition[a2];\
            [a2][big_a]overlay=0:0[a3];\
            [a3][ghost_a]overlay=format=auto:alpha=0.05[a4];\
            [a4][small_a]overlay=0:0[final_a];\
            [base_m][scan1_m]blend=all_mode=multiply[m1];\
            [m1][scan2_m]blend=all_mode=multiply[m2];\
            [m2][big_m]overlay=0:0[m3];\
            [m3][ghost_m]overlay=format=auto:alpha=0.05[m4];\
            [m4][small_m]overlay=0:0[final_m]\
          " \
          -map "[base_out]"        -frames:v 1 "${dir}/base.png" \
          -map "[rainbow_show]"    -frames:v 1 "${dir}/stage_rainbow.png" \
          -map "[rainbow_masked]"  -frames:v 1 "${dir}/masked_rainbow.png" \
          -map "[rainbow_for_map]" -frames:v 1 "${dir}/masked_rainbow2.png" \
          -map "[scan1_out]"       -frames:v 1 "${dir}/scan1.png" \
          -map "[scan2_out]"       -frames:v 1 "${dir}/scan2.png" \
          -map "[big_out]"         -frames:v 1 "${dir}/brand_big.png" \
          -map "[small_out]"       -frames:v 1 "${dir}/brand_small.png" \
          -map "[label_out]"       -frames:v 1 "${dir}/label.png" \
          -map "[ghost_out]"       -frames:v 1 "${dir}/rgbghost.png" \
          -map "[stage_scan_out]"  -frames:v 1 "${dir}/stage_scan.png" \
          -map "[stage_scan_color_out]" -frames:v 1 "${dir}/stage_scan_brand.png" \
          -map "[final_s]"         -frames:v 1 "${dir}/final.png" \
          -map "[final_scanmult]"  -frames:v 1 "${dir}/final_scanmult.png" \
          -map "[final_a]"         -frames:v 1 "${dir}/final_add.png" \
          -map "[final_m]"         -frames:v 1 "${dir}/final_mult.png"
    done
}

for preset in "${variants[@]}"; do
    read -r name c1 c2 spacing1 spacing2 mode sat ctr ghost scroll_step <<< "${preset}"
    render_variant "${name}" "${c1}" "${c2}" "${spacing1}" "${spacing2}" "${mode}" "${sat}" "${ctr}" "${ghost}" "${scroll_step}"
done

echo "Variations written to ${OUT_ROOT}"
