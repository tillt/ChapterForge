//
//  mp4_muxer.cpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#include "mp4_muxer.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <limits>

#include "aac_extractor.hpp"
#include "chapter_timing.hpp"
#include "logging.hpp"
#include "mdat_writer.hpp"
#include "meta_builder.hpp"
#include "moov_builder.hpp"
#include "mp4_atoms.hpp"
#include "stbl_audio_builder.hpp"
#include "stbl_image_builder.hpp"
#include "stbl_text_builder.hpp"
#include "trak_builder.hpp"
#include "udta_builder.hpp"

// Build an audio chunking plan. We previously mirrored the golden sample’s.
// 22/21 pattern; this version uses a consistent chunk size to see if Apple.
// players remain happy without the quirky first-chunk bump.
static std::vector<uint32_t> build_audio_chunk_plan(uint32_t sample_count) {
    std::vector<uint32_t> chunks;
    if (sample_count == 0) {
        return chunks;
    }

    const uint32_t chunk = 21;
    uint32_t remaining = sample_count;
    while (remaining > chunk) {
        chunks.push_back(chunk);
        remaining -= chunk;
    }
    if (remaining > 0) {
        chunks.push_back(remaining);
    }
    return chunks;
}

// Derive samples-per-chunk plan from stsc payload.
static std::vector<uint32_t> derive_chunk_plan(const std::vector<uint8_t> &stsc_payload,
                                               uint32_t sample_count) {
    std::vector<uint32_t> plan;
    if (stsc_payload.size() < 16) {
        return plan;
    }
    uint32_t entry_count = (stsc_payload[4] << 24) | (stsc_payload[5] << 16) |
                           (stsc_payload[6] << 8) | stsc_payload[7];
    size_t pos = 8;
    uint32_t consumed = 0;
    for (uint32_t i = 0; i < entry_count; ++i) {
        if (pos + 12 > stsc_payload.size()) {
            break;
        }
        uint32_t first_chunk = (stsc_payload[pos] << 24) | (stsc_payload[pos + 1] << 16) |
                               (stsc_payload[pos + 2] << 8) | stsc_payload[pos + 3];
        uint32_t samples_per_chunk = (stsc_payload[pos + 4] << 24) | (stsc_payload[pos + 5] << 16) |
                                     (stsc_payload[pos + 6] << 8) | stsc_payload[pos + 7];
        uint32_t next_first = 0;
        if (i + 1 < entry_count && pos + 12 <= stsc_payload.size() - 4) {
            next_first = (stsc_payload[pos + 12] << 24) | (stsc_payload[pos + 13] << 16) |
                         (stsc_payload[pos + 14] << 8) | (stsc_payload[pos + 15]);
        }
        if (samples_per_chunk == 0) {
            break;
        }
        uint32_t chunk_count = (next_first > 0 && next_first > first_chunk)
                                   ? (next_first - first_chunk)
                                   : 0;
        if (chunk_count == 0) {
            while (consumed < sample_count) {
                plan.push_back(samples_per_chunk);
                consumed += samples_per_chunk;
            }
        } else {
            for (uint32_t c = 0; c < chunk_count && consumed < sample_count; ++c) {
                plan.push_back(samples_per_chunk);
                consumed += samples_per_chunk;
            }
        }
        pos += 12;
    }
    // Clamp plan length to sample_count to avoid runaway.
    if (consumed > sample_count) {
        while (!plan.empty() && consumed > sample_count) {
            consumed -= plan.back();
            plan.pop_back();
        }
    }
    return plan;
}

// -----------------------------------------------------------------------------
// Minimal JPEG dimension parser (SOF0/1/2/3/5/6/7/9/10/11/12/13/14/15)
// -----------------------------------------------------------------------------
static bool parse_jpeg_dimensions(const std::vector<uint8_t> &data, uint16_t &width,
                                  uint16_t &height) {
    if (data.size() < 10 || data[0] != 0xFF || data[1] != 0xD8) {
        return false;  // not a JPEG SOI
    }

    size_t i = 2;
    while (i + 3 < data.size()) {
        if (data[i] != 0xFF) {
            ++i;
            continue;
        }
        uint8_t marker = data[i + 1];
        // Skip padding FFs.
        if (marker == 0xFF) {
            ++i;
            continue;
        }

        // EOI or SOS ends the searchable header section.
        if (marker == 0xD9 || marker == 0xDA) {
            break;
        }

        uint16_t seg_len = (static_cast<uint16_t>(data[i + 2]) << 8) | data[i + 3];
        if (seg_len < 2 || i + 2 + seg_len > data.size()) {
            break;
        }

        bool is_sof = (marker >= 0xC0 && marker <= 0xC3) || (marker >= 0xC5 && marker <= 0xC7) ||
                      (marker >= 0xC9 && marker <= 0xCB) || (marker >= 0xCD && marker <= 0xCF);
        if (is_sof && seg_len >= 7) {
            height = (static_cast<uint16_t>(data[i + 5]) << 8) | data[i + 6];
            width = (static_cast<uint16_t>(data[i + 7]) << 8) | data[i + 8];
            return true;
        }

        i += 2 + seg_len;
    }
    return false;
}

// -----------------------------------------------------------------------------
// Complete MP4 writer.
// -----------------------------------------------------------------------------
bool write_mp4(const std::string &output_path, const AacExtractResult &aac,
               const std::vector<ChapterTextSample> &text_chapters,
               const std::vector<ChapterImageSample> &image_chapters, Mp4aConfig audio_cfg,
               const MetadataSet &metadata, bool fast_start,
               const std::vector<std::pair<std::string, std::vector<ChapterTextSample>>>
                   &extra_text_tracks,
               const std::vector<uint8_t> *ilst_payload) {
    //
    // 0) open file.
    //
    std::ofstream out(output_path, std::ios::binary);
    if (!out.is_open()) {
        return false;
    }

    //
    // Write ftyp (once at head)
    //
    static const uint8_t ftyp_box[] = {0x00, 0x00, 0x00, 0x24, 'f',  't',  'y',  'p', 'M',
                                       '4',  'V',  ' ',  0x00, 0x00, 0x00, 0x01, 'm', 'p',
                                       '4',  '2',  'i',  's',  'o',  'm',  'M',  '4', 'A',
                                       ' ',  'M',  '4',  'V',  ' ',  'd',  'b',  'y', '1'};
    const uint32_t ftyp_size = sizeof(ftyp_box);
    out.write(reinterpret_cast<const char *>(ftyp_box), sizeof(ftyp_box));

    const uint32_t audio_sample_count = (uint32_t)aac.frames.size();
    if (audio_sample_count == 0) {
        throw std::runtime_error("No AAC frames extracted.");
    }

    //
    // Build audio samples for mdat.
    //
    std::vector<std::vector<uint8_t>> audio_samples = aac.frames;

    //
    // Text samples: just raw UTF-8 data.
    //
    auto encode_tx3g = [](const ChapterTextSample &sample) {
        std::vector<uint8_t> out;
        uint16_t len = static_cast<uint16_t>(sample.text.size());
        write_u16(out, len);
        out.insert(out.end(), sample.text.begin(), sample.text.end());
        if (!sample.href.empty()) {
            uint8_t url_len = static_cast<uint8_t>(std::min<size_t>(sample.href.size(), 255));
            uint16_t start = 0;
            uint16_t end = 0x000a;  // observed in golden samples
            uint32_t box_size = 4 + 4 + 2 + 2 + 1 + url_len + 1;  // +1 pad
            write_u32(out, box_size);
            out.push_back('h');
            out.push_back('r');
            out.push_back('e');
            out.push_back('f');
            write_u16(out, start);
            write_u16(out, end);
            out.push_back(url_len);
            out.insert(out.end(), sample.href.begin(), sample.href.begin() + url_len);
            out.push_back(0x00);
        }
        return out;
    };

    std::vector<std::vector<uint8_t>> text_samples;
    text_samples.reserve(text_chapters.size() + 1);
    for (auto &tx : text_chapters) {
        text_samples.emplace_back(encode_tx3g(tx));
    }
    if (!text_chapters.empty()) {
        text_samples.emplace_back(encode_tx3g(text_chapters.back()));  // pad trailing sample
    }
    std::vector<std::vector<std::vector<uint8_t>>> extra_text_samples;
    extra_text_samples.reserve(extra_text_tracks.size());
    for (const auto &track : extra_text_tracks) {
        std::vector<std::vector<uint8_t>> samples;
        samples.reserve(track.second.size() + 1);
        for (const auto &tx : track.second) {
            samples.emplace_back(encode_tx3g(tx));
        }
        if (!track.second.empty()) {
            samples.emplace_back(encode_tx3g(track.second.back()));  // pad trailing sample
        }
        extra_text_samples.push_back(std::move(samples));
    }

    //
    // Image samples: JPEG binary data (may be empty if no images were provided)
    //
    std::vector<std::vector<uint8_t>> image_samples;
    for (auto &im : image_chapters) {
        image_samples.push_back(im.data);
    }

    //
    //
    // 3) Track timing — we align audio and chapter tracks.
    //
    // populate audio config from parsed ADTS if available.
    audio_cfg.sample_rate = aac.sample_rate ? aac.sample_rate : audio_cfg.sample_rate;
    audio_cfg.channel_count = aac.channel_config ? aac.channel_config : audio_cfg.channel_count;
    audio_cfg.sampling_index = aac.sampling_index ? aac.sampling_index : audio_cfg.sampling_index;
    audio_cfg.audio_object_type =
        aac.audio_object_type ? aac.audio_object_type : audio_cfg.audio_object_type;

    const uint32_t audio_timescale = audio_cfg.sample_rate;  // typically 44100
    const uint64_t audio_duration_ts =
        (uint64_t)audio_sample_count * 1024;  // AAC LC = 1024 PCM samples/frame
    const uint32_t audio_duration_ms =
        static_cast<uint32_t>((audio_duration_ts * 1000 + audio_timescale - 1) / audio_timescale);

    // For text + image tracks we use 1000 Hz timescale (ms resolution).
    const uint32_t chapter_timescale = 1000;
    auto text_durations = derive_durations_ms_from_starts(text_chapters, audio_duration_ms);
    auto image_durations = derive_durations_ms_from_starts(image_chapters, audio_duration_ms);
    uint64_t text_duration_ts = 0;
    uint64_t image_duration_ts = 0;
    for (auto dur_ms : text_durations) {
        text_duration_ts += (uint64_t)dur_ms * chapter_timescale / 1000;
    }
    for (auto dur_ms : image_durations) {
        image_duration_ts += (uint64_t)dur_ms * chapter_timescale / 1000;
    }

    CH_LOG("info", "[durations] text_ts=" << text_duration_ts << " image_ts=" << image_duration_ts
                                          << " audio_ts=" << audio_duration_ts
                                          << " (audio_timescale=" << audio_timescale << ")");

    //
    // 4) Build STBL for each track.
    //
    std::vector<uint32_t> audio_chunk_plan =
        aac.stsc_payload.empty() ? build_audio_chunk_plan(audio_sample_count)
                                 : derive_chunk_plan(aac.stsc_payload, audio_sample_count);
    std::vector<uint32_t> text_chunk_plan(text_samples.size(), 1);
    std::vector<std::vector<uint32_t>> extra_text_chunk_plans;
    extra_text_chunk_plans.reserve(extra_text_samples.size());
    for (const auto &samples : extra_text_samples) {
        extra_text_chunk_plans.emplace_back(samples.size(), 1);
    }
    // Keep image chunks simple: 1 sample per chunk to avoid stsc/stco
    // inconsistencies seen by QuickTime/ffmpeg.
    std::vector<uint32_t> image_chunk_plan(image_samples.size(), 1);

    // Aggregate text tracks (primary + extras) for mdat/offset handling.
    std::vector<std::vector<std::vector<uint8_t>>> all_text_samples;
    all_text_samples.push_back(text_samples);
    all_text_samples.insert(all_text_samples.end(), extra_text_samples.begin(),
                            extra_text_samples.end());
    std::vector<std::vector<uint32_t>> all_text_chunk_plans;
    all_text_chunk_plans.push_back(text_chunk_plan);
    all_text_chunk_plans.insert(all_text_chunk_plans.end(), extra_text_chunk_plans.begin(),
                                extra_text_chunk_plans.end());

    // Compute image width/height from the first JPEG when available to match.
    // encoded size.
    uint16_t image_width = 1280;
    uint16_t image_height = 720;
    bool has_image_track = !image_samples.empty();
    if (has_image_track) {
        uint16_t w = 0, h = 0;
        if (parse_jpeg_dimensions(image_samples.front(), w, h) && w > 0 && h > 0) {
            image_width = w;
            image_height = h;
        }
    }

    // Pre-build stbls (needed for both fast-start and normal paths)
    std::unique_ptr<Atom> stbl_audio;
    if (!aac.stsd_payload.empty() && !aac.stts_payload.empty() && !aac.stsc_payload.empty() &&
        !aac.stsz_payload.empty() && !aac.stco_payload.empty()) {
        // Golden-aligned path: reuse the source audio stbl verbatim. Rebuilding.
        // stbl for audio triggered decoding issues in Apple players even when.
        // the fields were “correct” per spec, so we preserve the original.
        // structure whenever we can.
        CH_LOG("info", "Reusing source audio stbl");
        stbl_audio = build_audio_stbl_raw(aac.stsd_payload, aac.stts_payload, aac.stsc_payload,
                                          aac.stsz_payload, aac.stco_payload);
    } else {
        CH_LOG("info", "Building new audio stbl");
        stbl_audio =
            build_audio_stbl(audio_cfg, aac.sizes, audio_chunk_plan, audio_sample_count, nullptr);
    }

    auto stbl_text = build_text_stbl(text_chapters, chapter_timescale, text_chunk_plan,
                                     audio_duration_ms);

    std::unique_ptr<Atom> stbl_image;
    if (has_image_track) {
        stbl_image = build_image_stbl(image_chapters, chapter_timescale, image_width, image_height,
                                      image_chunk_plan, audio_duration_ms);
    }

    //
    // 6) Build tracks.
    //
    const uint32_t AUDIO_TRACK_ID = 1;
    const uint32_t TEXT_TRACK_ID = 2;
    const uint32_t IMAGE_TRACK_ID =
        TEXT_TRACK_ID + 1 + static_cast<uint32_t>(extra_text_tracks.size());

    const uint32_t mvhd_timescale = 600;
    const uint64_t tkhd_audio_duration = (audio_duration_ts * mvhd_timescale) / audio_timescale;
    const uint64_t tkhd_chapter_duration = (text_duration_ts * mvhd_timescale) / chapter_timescale;
    const uint64_t tkhd_image_duration =
        has_image_track ? (image_duration_ts * mvhd_timescale) / chapter_timescale : 0;
    const uint64_t mvhd_duration =
        has_image_track ? std::max({tkhd_audio_duration, tkhd_chapter_duration, tkhd_image_duration})
                        : std::max(tkhd_audio_duration, tkhd_chapter_duration);

    // Reference title text track (and image if present) in tref/chap; omit URL track to keep QT happy.
    std::vector<uint32_t> chapter_refs;
    chapter_refs.push_back(TEXT_TRACK_ID);
    if (has_image_track) {
        chapter_refs.push_back(IMAGE_TRACK_ID);
    }

    auto trak_audio = build_trak_audio(AUDIO_TRACK_ID, audio_timescale, audio_duration_ts,
                                       std::move(stbl_audio), chapter_refs, tkhd_audio_duration);

    std::vector<std::unique_ptr<Atom>> text_traks;
    // Make title track enabled/visible (QuickTime shows titles when chap -> enabled text track).
    text_traks.push_back(build_trak_text(TEXT_TRACK_ID, chapter_timescale, text_duration_ts,
                                         std::move(stbl_text), tkhd_chapter_duration,
                                         "Chapter Titles", true));

    for (size_t i = 0; i < extra_text_tracks.size(); ++i) {
        uint32_t tid = TEXT_TRACK_ID + 1 + static_cast<uint32_t>(i);
        auto stbl_extra = build_text_stbl(extra_text_tracks[i].second, chapter_timescale,
                                          extra_text_chunk_plans[i], audio_duration_ms);
        text_traks.push_back(build_trak_text(tid, chapter_timescale, text_duration_ts,
                                             std::move(stbl_extra), tkhd_chapter_duration,
                                             extra_text_tracks[i].first, true));
    }

    std::unique_ptr<Atom> trak_image;
    if (has_image_track) {
        trak_image =
            build_trak_image(IMAGE_TRACK_ID, chapter_timescale, image_duration_ts,
                             std::move(stbl_image), image_width, image_height, tkhd_image_duration);
    }

    //
    // 7) Build udta/meta/ilst block for cover art, metadata, and chpl.
    //
    std::unique_ptr<Atom> meta_atom;
    if (ilst_payload && !ilst_payload->empty()) {
        meta_atom = build_meta_atom_from_ilst(*ilst_payload);
    } else {
        meta_atom = build_meta_atom(metadata);
    }
    auto udta = build_udta(std::move(meta_atom), text_chapters);

    //
    // 8) Build moov.
    //
    auto moov = build_moov(mvhd_timescale, mvhd_duration, std::move(trak_audio),
                           std::move(text_traks), std::move(trak_image), std::move(udta));
    moov->fix_size_recursive();

    if (fast_start) {
        // moov before mdat: compute offsets assuming mdat follows immediately.
        // after moov. This fast-start layout mirrors the golden file and keeps.
        // Apple players happy; other valid layouts have shown sporadic playback.
        // regressions despite being spec-compliant.
        uint64_t payload_start =
            static_cast<uint64_t>(ftyp_size) + moov->size() + 8;  // +8 for mdat header
        MdatOffsets mdat_offs =
            compute_mdat_offsets(payload_start, audio_samples, all_text_samples, image_samples,
                                 audio_chunk_plan, all_text_chunk_plans, image_chunk_plan);
        patch_all_stco(moov.get(), mdat_offs, true);

        // write moov, then mdat.
        moov->write(out);
        write_mdat(out, audio_samples, all_text_samples, image_samples, audio_chunk_plan,
                   all_text_chunk_plans, image_chunk_plan);
    } else {
        // Write mdat first and capture offsets.
        MdatOffsets mdat_offs = write_mdat(out, audio_samples, all_text_samples, image_samples,
                                           audio_chunk_plan, all_text_chunk_plans,
                                           image_chunk_plan);

        // Patch STCO in moov using the actual offsets from written mdat.
        // If we reused the source audio stco, skip patching it.
        patch_all_stco(moov.get(), mdat_offs, true);

        // Optional leading free box before moov (padding). Size/placement.
        // mirrors the golden sample to avoid surprising atom ordering.
        // sensitivities.
        {
            auto free = Atom::create("free");
            free->payload.resize(1024, 0);
            free->fix_size_recursive();
            free->write(out);
        }

        // Write moov at end of file.
        moov->fix_size_recursive();
        moov->write(out);
    }

    return true;
}
