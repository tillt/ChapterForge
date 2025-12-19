//
//  mp4_muxer.cpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#include "mp4_muxer.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <limits>

#include "aac_extractor.hpp"
#include "chapter_timing.hpp"
#include "logging.hpp"
#include "mdat_writer.hpp"
#include "jpeg_info.hpp"
#include "metadata_set.hpp"
#include "chapterforge_version.hpp"
#include "meta_builder.hpp"
#include "moov_builder.hpp"
#include "mp4_atoms.hpp"
#include "stbl_audio_builder.hpp"
#include "stbl_image_builder.hpp"
#include "stbl_text_builder.hpp"
#include "trak_builder.hpp"
#include "udta_builder.hpp"

using chapterforge::hex_prefix;

namespace {

constexpr uint32_t kAacSamplesPerFrame = 1024;     // AAC LC = 1024 PCM samples/frame
constexpr uint32_t kChapterTimescale = 1000;       // ms resolution for text/image tracks
// Fallback when a JPEG carries no decodable size: mirror the 1280x720 track dimensions
// seen in the Apple-authored “golden” samples to keep player expectations aligned.
constexpr uint16_t kDefaultImageWidth = 1280;
constexpr uint16_t kDefaultImageHeight = 720;
constexpr uint32_t kDefaultAudioChunk = 21;        // chunk size used for derived plans
constexpr uint32_t kStscHeaderSize = 8;
constexpr uint32_t kStscEntrySize = 12;

}  // namespace

// Build an audio chunking plan. We previously mirrored the golden sample’s 22/21 pattern; this
// version uses a consistent chunk size to see if Apple players remain happy without the quirky
// first-chunk bump.
static std::vector<uint32_t> build_audio_chunk_plan(uint32_t sample_count) {
    std::vector<uint32_t> chunks;
    if (sample_count == 0) {
        return chunks;
    }

    uint32_t remaining = sample_count;
    while (remaining > kDefaultAudioChunk) {
        chunks.push_back(kDefaultAudioChunk);
        remaining -= kDefaultAudioChunk;
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
    if (stsc_payload.size() < kStscHeaderSize + kStscEntrySize) {
        return plan;
    }
    uint32_t entry_count = (stsc_payload[4] << 24) | (stsc_payload[5] << 16) |
                           (stsc_payload[6] << 8) | stsc_payload[7];
    size_t pos = kStscHeaderSize;
    uint32_t consumed = 0;
    for (uint32_t i = 0; i < entry_count; ++i) {
        if (pos + kStscEntrySize > stsc_payload.size()) {
            break;
        }
        uint32_t first_chunk = (stsc_payload[pos] << 24) | (stsc_payload[pos + 1] << 16) |
                               (stsc_payload[pos + 2] << 8) | stsc_payload[pos + 3];
        uint32_t samples_per_chunk = (stsc_payload[pos + 4] << 24) | (stsc_payload[pos + 5] << 16) |
                                     (stsc_payload[pos + 6] << 8) | stsc_payload[pos + 7];
        uint32_t next_first = 0;
        if (i + 1 < entry_count && pos + kStscEntrySize <= stsc_payload.size() - 4) {
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

struct DurationInfo {
    uint32_t audio_timescale = 0;
    uint64_t audio_duration_ts = 0;
    uint32_t audio_duration_ms = 0;
    uint64_t text_duration_ts = 0;
    uint64_t image_duration_ts = 0;
    std::vector<uint32_t> text_ms;
    std::vector<uint32_t> image_ms;
};

struct PreparedTextTracks {
    std::vector<ChapterTextSample> primary_meta;
    std::vector<std::vector<uint8_t>> primary;
    std::vector<std::vector<ChapterTextSample>> extras_meta;
    std::vector<std::vector<std::vector<uint8_t>>> extras;
};

struct ChunkPlans {
    std::vector<uint32_t> audio;
    std::vector<std::vector<uint32_t>> text;
    std::vector<uint32_t> image;
};

static std::vector<uint8_t> encode_tx3g_sample(const ChapterTextSample &sample) {
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
}

static std::vector<std::vector<uint8_t>> encode_tx3g_track(
    const std::vector<ChapterTextSample> &chapters) {
    std::vector<std::vector<uint8_t>> samples;
    samples.reserve(chapters.size());
    for (const auto &tx : chapters) {
        samples.emplace_back(encode_tx3g_sample(tx));
    }
    return samples;
}

static PreparedTextTracks prepare_text_tracks(
    const std::vector<ChapterTextSample> &text_chapters,
    const std::vector<std::pair<std::string, std::vector<ChapterTextSample>>> &extra_text_tracks) {
    PreparedTextTracks prepared;

    prepared.primary_meta = text_chapters;
    prepared.primary = encode_tx3g_track(prepared.primary_meta);

    prepared.extras_meta.reserve(extra_text_tracks.size());
    prepared.extras.reserve(extra_text_tracks.size());
    for (const auto &track : extra_text_tracks) {
        prepared.extras_meta.push_back(track.second);
        prepared.extras.push_back(encode_tx3g_track(prepared.extras_meta.back()));
    }
    return prepared;
}

static DurationInfo compute_durations(const AacExtractResult &aac, Mp4aConfig &audio_cfg,
                                      const std::vector<ChapterTextSample> &text_chapters,
                                      const std::vector<ChapterImageSample> &image_chapters) {
    DurationInfo info;

    // populate audio config from parsed ADTS if available.
    audio_cfg.sample_rate = aac.sample_rate ? aac.sample_rate : audio_cfg.sample_rate;
    audio_cfg.channel_count = aac.channel_config ? aac.channel_config : audio_cfg.channel_count;
    audio_cfg.sampling_index = aac.sampling_index ? aac.sampling_index : audio_cfg.sampling_index;
    audio_cfg.audio_object_type =
        aac.audio_object_type ? aac.audio_object_type : audio_cfg.audio_object_type;

    info.audio_timescale = audio_cfg.sample_rate;
    info.audio_duration_ts = static_cast<uint64_t>(aac.frames.size()) * kAacSamplesPerFrame;
    info.audio_duration_ms =
        static_cast<uint32_t>((info.audio_duration_ts * 1000 + info.audio_timescale - 1) /
                              info.audio_timescale);

    info.text_ms = derive_durations_ms_from_starts(text_chapters, info.audio_duration_ms);
    info.image_ms = derive_durations_ms_from_starts(image_chapters, info.audio_duration_ms);
    for (auto dur_ms : info.text_ms) {
        info.text_duration_ts += (uint64_t)dur_ms * kChapterTimescale / 1000;
    }
    for (auto dur_ms : info.image_ms) {
        info.image_duration_ts += (uint64_t)dur_ms * kChapterTimescale / 1000;
    }
    return info;
}

static bool first_image_dimensions_and_check(const std::vector<std::vector<uint8_t>> &image_samples,
                                             uint16_t &image_width, uint16_t &image_height) {
    if (image_samples.empty()) {
        return true;
    }
    uint16_t w = 0, h = 0;
    bool is_yuv420 = false;
    if (!parse_jpeg_info(image_samples.front(), w, h, is_yuv420) || w == 0 || h == 0) {
        CH_LOG("error", "failed to parse JPEG header for chapter image 0; ensure valid JPEG");
        return false;
    }
    image_width = w;
    image_height = h;
    if (!is_yuv420) {
        CH_LOG("error", "chapter image is not 4:2:0 (yuvj420p); re-encode with -pix_fmt yuvj420p");
        return false;
    }
    CH_LOG("debug", "chapter image subsampling OK (yuv420)");
    return true;
}

static bool validate_additional_images(const std::vector<std::vector<uint8_t>> &image_samples,
                                       uint16_t image_width, uint16_t image_height) {
    for (size_t i = 1; i < image_samples.size(); ++i) {
        uint16_t wi = 0, hi = 0;
        bool dummy_yuv = false;
        if (parse_jpeg_info(image_samples[i], wi, hi, dummy_yuv) && wi > 0 && hi > 0) {
            if (wi != image_width || hi != image_height) {
                CH_LOG("warn", "chapter image " << i << " has dimensions " << wi << "x" << hi
                                                << " differing from first image " << image_width
                                                << "x" << image_height
                                                << "; Apple players may only display the first");
                break;  // warn once
            }
            if (!dummy_yuv) {
                CH_LOG("error",
                       "chapter image " << i << " is not 4:2:0 (yuvj420p); re-encode the JPEGs");
                return false;
            }
        }
    }
    return true;
}

static ChunkPlans build_chunk_plans(const AacExtractResult &aac, uint32_t audio_sample_count,
                                    const PreparedTextTracks &texts,
                                    const std::vector<std::vector<uint8_t>> &image_samples) {
    ChunkPlans plans;
    plans.audio =
        aac.stsc_payload.empty() ? build_audio_chunk_plan(audio_sample_count)
                                 : derive_chunk_plan(aac.stsc_payload, audio_sample_count);
    plans.text.push_back(std::vector<uint32_t>(texts.primary.size(), 1));
    for (const auto &samples : texts.extras) {
        plans.text.emplace_back(samples.size(), 1);
    }
    plans.image = std::vector<uint32_t>(image_samples.size(), 1);
    return plans;
}

static void log_inputs(const MetadataSet &metadata,
                       const std::vector<ChapterTextSample> &text_chapters,
                       const std::vector<std::pair<std::string, std::vector<ChapterTextSample>>>
                           &extra_text_tracks,
                       const std::vector<ChapterImageSample> &image_chapters) {
    CH_LOG("debug", "metadata title='" << metadata.title << "' artist='" << metadata.artist
                                       << "' album='" << metadata.album
                                       << "' genre='" << metadata.genre << "' year='" << metadata.year
                                       << "' comment='" << metadata.comment << "' cover_bytes="
                                       << metadata.cover.size() << " cover_hex="
                                       << (metadata.cover.empty() ? "<none>"
                                                                  : hex_prefix(metadata.cover)));

    auto log_text_track = [](const char *label, const std::vector<ChapterTextSample> &samples) {
        for (size_t i = 0; i < samples.size(); ++i) {
            const auto &s = samples[i];
            CH_LOG("debug", label << "[" << i << "] start_ms=" << s.start_ms
                                  << " text=\"" << s.text << "\" href="
                                  << (s.href.empty() ? "<none>" : s.href));
        }
    };
    log_text_track("titles", text_chapters);
    for (size_t i = 0; i < extra_text_tracks.size(); ++i) {
        log_text_track(extra_text_tracks[i].first.c_str(), extra_text_tracks[i].second);
    }
    for (size_t i = 0; i < image_chapters.size(); ++i) {
        const auto &img = image_chapters[i];
        CH_LOG("debug", "image[" << i << "] start_ms=" << img.start_ms << " bytes="
                                 << img.data.size() << " hex=" << hex_prefix(img.data));
    }
}

#ifdef CHAPTERFORGE_TESTING
namespace chapterforge::testing {
std::vector<uint32_t> build_audio_chunk_plan_for_test(uint32_t sample_count) {
    return build_audio_chunk_plan(sample_count);
}
std::vector<uint32_t> derive_chunk_plan_for_test(const std::vector<uint8_t> &stsc_payload,
                                                 uint32_t sample_count) {
    return derive_chunk_plan(stsc_payload, sample_count);
}
std::vector<uint8_t> encode_tx3g_sample_for_test(const ChapterTextSample &sample) {
    return encode_tx3g_sample(sample);
}
std::vector<std::vector<uint8_t>> encode_tx3g_track_for_test(
    const std::vector<ChapterTextSample> &chapters) {
    return encode_tx3g_track(chapters);
}
TestDurationInfo compute_durations_for_test(
    const AacExtractResult &aac, Mp4aConfig audio_cfg,
    const std::vector<ChapterTextSample> &text_chapters,
    const std::vector<ChapterImageSample> &image_chapters) {
    auto info = compute_durations(aac, audio_cfg, text_chapters, image_chapters);
    TestDurationInfo out;
    out.audio_timescale = info.audio_timescale;
    out.audio_duration_ts = info.audio_duration_ts;
    out.audio_duration_ms = info.audio_duration_ms;
    out.text_ms = info.text_ms;
    out.image_ms = info.image_ms;
    return out;
}
}  // namespace chapterforge::testing
#endif

// -----------------------------------------------------------------------------
// Complete MP4 writer.
// -----------------------------------------------------------------------------
bool write_mp4(const std::string &output_path, const AacExtractResult &aac,
               const std::vector<ChapterTextSample> &text_chapters,
               const std::vector<ChapterImageSample> &image_chapters, Mp4aConfig audio_cfg,
               const MetadataSet &metadata, bool fast_start,
               const std::vector<std::pair<std::string, std::vector<ChapterTextSample>>>
                   &extra_text_tracks,
               const std::vector<uint8_t> *ilst_payload,
               const std::vector<uint8_t> *meta_payload) {
    CH_LOG("debug", "write_mp4 begin output=" << output_path << " audio_frames=" << aac.frames.size()
                                              << " titles=" << text_chapters.size()
                                             << " images=" << image_chapters.size()
                                             << " extra_text_tracks=" << extra_text_tracks.size()
                                             << " fast_start=" << fast_start);
    auto now = [] { return std::chrono::steady_clock::now(); };
    auto meta_is_empty = [](const MetadataSet &m) {
        return m.title.empty() && m.artist.empty() && m.album.empty() && m.genre.empty() &&
               m.year.empty() && m.comment.empty() && m.cover.empty();
    };
    auto t_start = now();

    log_inputs(metadata, text_chapters, extra_text_tracks, image_chapters);
    //
    // 0) open file.
    //
    std::ofstream out(output_path, std::ios::binary);
    if (!out.is_open()) {
        CH_LOG("error", "Failed to open output for write: " << output_path);
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
    CH_LOG("debug", "audio sample count=" << audio_sample_count);

    //
    // Build audio samples for mdat.
    //
    const auto &audio_samples = aac.frames;  // reuse extracted buffers without copying

    // Build padded tx3g samples for title and URL tracks.
    PreparedTextTracks prepared_text = prepare_text_tracks(text_chapters, extra_text_tracks);

    //
    // Image samples: JPEG binary data (may be empty if no images were provided)
    //
    std::vector<std::vector<uint8_t>> image_samples;
    for (auto &im : image_chapters) {
        image_samples.push_back(im.data);
    }
    CH_LOG("debug", "text samples=" << prepared_text.primary.size()
                                    << " image samples=" << image_samples.size());
    CH_LOG("debug", "primary_meta count=" << prepared_text.primary_meta.size()
                                          << " starts="
                                          << (prepared_text.primary_meta.empty()
                                                  ? std::string("<none>")
                                                  : std::to_string(
                                                        prepared_text.primary_meta.front().start_ms))
                                          << (prepared_text.primary_meta.size() > 1
                                                  ? ("..." + std::to_string(
                                                         prepared_text.primary_meta.back().start_ms))
                                                  : ""));

    //
    //
    // 3) Track timing — we align audio and chapter tracks.
    //
    DurationInfo durations =
        compute_durations(aac, audio_cfg, prepared_text.primary_meta, image_chapters);
    CH_LOG("debug", "audio cfg sr=" << audio_cfg.sample_rate
                                    << " ch=" << audio_cfg.channel_count
                                    << " obj=" << audio_cfg.audio_object_type
                                    << " duration_ts=" << durations.audio_duration_ts);
    CH_LOG("debug", "[durations] text_ts=" << durations.text_duration_ts
                                           << " image_ts=" << durations.image_duration_ts
                                           << " audio_ts=" << durations.audio_duration_ts
                                           << " (audio_timescale=" << durations.audio_timescale
                                           << ")");
    CH_LOG("debug", "derived text durations=" << durations.text_ms.size()
                                              << " image durations=" << durations.image_ms.size());

    //
    // 4) Build STBL for each track.
    //
    ChunkPlans chunk_plans =
        build_chunk_plans(aac, audio_sample_count, prepared_text, image_samples);

    // Aggregate text tracks (primary + extras) for mdat/offset handling.
    std::vector<std::vector<std::vector<uint8_t>>> all_text_samples;
    all_text_samples.push_back(prepared_text.primary);
    all_text_samples.insert(all_text_samples.end(), prepared_text.extras.begin(),
                            prepared_text.extras.end());
    std::vector<std::vector<uint32_t>> all_text_chunk_plans = chunk_plans.text;

    CH_LOG("debug", "chunk plans: audio=" << chunk_plans.audio.size()
                                           << " text=" << all_text_chunk_plans.size()
                                           << " image=" << chunk_plans.image.size());

    // Compute image width/height from the first JPEG when available to match encoded size.
    uint16_t image_width = kDefaultImageWidth;
    uint16_t image_height = kDefaultImageHeight;
    bool has_image_track = !image_samples.empty();
    if (has_image_track) {
        if (!first_image_dimensions_and_check(image_samples, image_width, image_height)) {
            return false;
        }
        if (!validate_additional_images(image_samples, image_width, image_height)) {
            return false;
        }
    }
    auto t_prep_end = now();

    // Pre-build stbls (needed for both fast-start and normal paths)
    std::unique_ptr<Atom> stbl_audio;
    if (!aac.stsd_payload.empty() && !aac.stts_payload.empty() && !aac.stsc_payload.empty() &&
        !aac.stsz_payload.empty() && !aac.stco_payload.empty()) {
        // Golden-aligned path: reuse the source audio stbl verbatim. Rebuilding stbl for audio
        // triggered decoding issues in Apple players even when the fields were “correct” per spec,
        // so we preserve the original structure whenever we can.
        CH_LOG("debug", "Reusing source audio stbl");
        stbl_audio = build_audio_stbl_raw(aac.stsd_payload, aac.stts_payload, aac.stsc_payload,
                                          aac.stsz_payload, aac.stco_payload);
    } else {
        CH_LOG("debug", "Building new audio stbl");
        stbl_audio =
            build_audio_stbl(audio_cfg, aac.sizes, chunk_plans.audio, audio_sample_count, nullptr);
    }

    auto stbl_text = build_text_stbl(prepared_text.primary_meta, kChapterTimescale,
                                     chunk_plans.text[0], durations.audio_duration_ms);

    std::unique_ptr<Atom> stbl_image;
    if (has_image_track) {
        stbl_image = build_image_stbl(image_chapters, kChapterTimescale, image_width, image_height,
                                      chunk_plans.image, durations.audio_duration_ms);
    }
    auto t_stbl_end = now();

    //
    // 6) Build tracks.
    //
    const uint32_t AUDIO_TRACK_ID = 1;
    const uint32_t TEXT_TRACK_ID = 2;
    const uint32_t IMAGE_TRACK_ID =
        TEXT_TRACK_ID + 1 + static_cast<uint32_t>(extra_text_tracks.size());

    const uint32_t mvhd_timescale = 600;
    const uint64_t tkhd_audio_duration =
        (durations.audio_duration_ts * mvhd_timescale) / durations.audio_timescale;
    const uint64_t tkhd_chapter_duration =
        (durations.text_duration_ts * mvhd_timescale) / kChapterTimescale;
    const uint64_t tkhd_image_duration =
        has_image_track ? (durations.image_duration_ts * mvhd_timescale) / kChapterTimescale : 0;
    const uint64_t mvhd_duration =
        has_image_track ? std::max({tkhd_audio_duration, tkhd_chapter_duration, tkhd_image_duration})
                        : std::max(tkhd_audio_duration, tkhd_chapter_duration);

    // Reference title text track (and image if present) in tref/chap; omit URL track to keep QT happy.
    std::vector<uint32_t> chapter_refs;
    chapter_refs.push_back(TEXT_TRACK_ID);
    if (has_image_track) {
        chapter_refs.push_back(IMAGE_TRACK_ID);
    }
    CH_LOG("debug", "tref chap refs count=" << chapter_refs.size());

    auto trak_audio = build_trak_audio(AUDIO_TRACK_ID, durations.audio_timescale,
                                       durations.audio_duration_ts, std::move(stbl_audio),
                                       chapter_refs, tkhd_audio_duration);

    std::vector<std::unique_ptr<Atom>> text_traks;
    // Make title track enabled/visible (QuickTime shows titles when chap -> enabled text track).
    text_traks.push_back(build_trak_text(TEXT_TRACK_ID, kChapterTimescale, durations.text_duration_ts,
                                         std::move(stbl_text), tkhd_chapter_duration,
                                         "Chapter Titles", true));

    for (size_t i = 0; i < extra_text_tracks.size(); ++i) {
        uint32_t tid = TEXT_TRACK_ID + 1 + static_cast<uint32_t>(i);
        auto stbl_extra = build_text_stbl(prepared_text.extras_meta[i], kChapterTimescale,
                                          chunk_plans.text[i + 1], durations.audio_duration_ms);
        text_traks.push_back(build_trak_text(tid, kChapterTimescale, durations.text_duration_ts,
                                             std::move(stbl_extra), tkhd_chapter_duration,
                                             extra_text_tracks[i].first, true));
    }

    std::unique_ptr<Atom> trak_image;
    if (has_image_track) {
        trak_image =
            build_trak_image(IMAGE_TRACK_ID, kChapterTimescale, durations.image_duration_ts,
                             std::move(stbl_image), image_width, image_height, tkhd_image_duration);
    }
    auto t_tracks_end = now();

    //
    // 7) Build udta/meta/ilst block for cover art, metadata, and chpl.
    //
    std::unique_ptr<Atom> meta_atom;
    if (meta_is_empty(metadata)) {
        if (meta_payload && !meta_payload->empty()) {
            meta_atom = Atom::create("meta");
            meta_atom->payload = *meta_payload;
        } else if (ilst_payload && !ilst_payload->empty()) {
            meta_atom = build_meta_atom_from_ilst(*ilst_payload);
        } else {
            meta_atom = build_meta_atom(metadata);
        }
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
    CH_LOG("debug", "moov size=" << moov->size() << " mvhd_duration=" << mvhd_duration);
    auto t_moov_end = now();

    auto t_layout_end = t_moov_end;
    auto t_write_end = t_moov_end;

    if (fast_start) {
        // Moov before mdat: compute offsets assuming mdat follows immediately after moov.
        // This fast-start layout mirrors the golden file and keeps Apple players happy;
        // other valid layouts have shown sporadic playback regressions despite being
        // spec-compliant.
        uint64_t payload_start =
            static_cast<uint64_t>(ftyp_size) + moov->size() + 8;  // +8 for mdat header
        MdatOffsets mdat_offs =
            compute_mdat_offsets(payload_start, audio_samples, all_text_samples, image_samples,
                                 chunk_plans.audio, all_text_chunk_plans, chunk_plans.image);
        patch_all_stco(moov.get(), mdat_offs, true);
        t_layout_end = now();

        // write moov, then mdat.
        moov->write(out);
        write_mdat(out, audio_samples, all_text_samples, image_samples, chunk_plans.audio,
                   all_text_chunk_plans, chunk_plans.image);
        t_write_end = now();
    } else {
        // Write mdat first and capture offsets.
        MdatOffsets mdat_offs = write_mdat(out, audio_samples, all_text_samples, image_samples,
                                           chunk_plans.audio, all_text_chunk_plans,
                                           chunk_plans.image);

        // Patch STCO in moov using the actual offsets from written mdat.
        // If we reused the source audio stco, skip patching it.
        patch_all_stco(moov.get(), mdat_offs, true);
        t_layout_end = now();

        // Optional leading free box before moov (padding). Size/placement mirrors the golden sample
        // to avoid surprising atom ordering sensitivities.
        {
            auto free = Atom::create("free");
            free->payload.resize(1024, 0);
            free->fix_size_recursive();
            free->write(out);
        }

        // Write moov at end of file.
        moov->fix_size_recursive();
        moov->write(out);
        t_write_end = now();
    }

    auto ms = [](auto a, auto b) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count();
    };
    CH_LOG("debug",
           "write_mp4 timings ms: prep=" << ms(t_start, t_prep_end)
                                         << " stbl=" << ms(t_prep_end, t_stbl_end)
                                         << " trak=" << ms(t_stbl_end, t_tracks_end)
                                         << " moov=" << ms(t_tracks_end, t_moov_end)
                                         << " layout=" << ms(t_moov_end, t_layout_end)
                                         << " write=" << ms(t_layout_end, t_write_end)
                                         << " total=" << ms(t_start, t_write_end));
    CH_LOG("debug", "ChapterForge version " << CHAPTERFORGE_VERSION_DISPLAY);

	return true;
}
