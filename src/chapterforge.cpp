//
//  chapterforge.cpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//
#include "chapterforge.hpp"
#include "chapterforge_version.hpp"

#include <filesystem>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>
#include <cctype>
#include <utility>
#include <chrono>

#include "aac_extractor.hpp"
#include "logging.hpp"
#include "metadata_set.hpp"
#include "chapter_text_sample.hpp"
#include "chapter_image_sample.hpp"
#include "mp4a_builder.hpp"
#include "mp4_muxer.hpp"
#include "parser.hpp"

using json = nlohmann::json;

namespace chapterforge {

std::string version_string() { return CHAPTERFORGE_VERSION_DISPLAY; }

}  // namespace chapterforge

namespace {

static bool read_file(const std::string &path, std::vector<uint8_t> &out) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        CH_LOG("error", "open failed for " << path << " errno=" << errno << " ("
                                           << std::generic_category().message(errno) << ")");
        return false;
    }
    f.seekg(0, std::ios::end);
    size_t sz = static_cast<size_t>(f.tellg());
    f.seekg(0, std::ios::beg);
    out.resize(sz);
    f.read(reinterpret_cast<char *>(out.data()), static_cast<std::streamsize>(sz));
    return true;
}

static std::vector<uint8_t> load_jpeg(const std::string &path) {
    std::vector<uint8_t> out;
    read_file(path, out);
    return out;
}

static std::optional<AacExtractResult> load_audio(const std::string &path) {
    // If the input is M4A/MP4, reuse stbl; if ADTS, decode to frames.
    auto ext = std::filesystem::path(path).extension().string();
    for (auto &c : ext) {
        c = static_cast<char>(::tolower(c));
    }
    if (ext == ".m4a" || ext == ".mp4") {
        return extract_from_mp4(path);
    }
    std::vector<uint8_t> bytes;
    if (!read_file(path, bytes)) {
        return std::nullopt;
    }
    auto res = extract_adts_frames(bytes);
    if (res.frames.empty()) {
        return std::nullopt;
    }
    return res;
}

inline uint32_t be32(const uint8_t *p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

constexpr uint32_t fourcc(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) |
           (static_cast<uint32_t>(c) << 8) | static_cast<uint32_t>(d);
}

// Minimal ilst parser to surface top-level metadata into MetadataSet.
static void parse_ilst_metadata(const std::vector<uint8_t> &ilst, MetadataSet &out) {
    auto extract_data_box = [](const uint8_t *base, size_t len,
                               std::vector<uint8_t> &payload_out, uint32_t &data_type_out) -> bool {
        size_t cursor = 0;
        while (cursor + 8 <= len) {
            const uint8_t *ptr = base + cursor;
            uint32_t sz = be32(ptr);
            if (sz < 8 || cursor + sz > len) {
                break;
            }
            uint32_t typ = be32(ptr + 4);
            if (typ == fourcc('d', 'a', 't', 'a') && sz >= 16) {
                data_type_out = be32(ptr + 8);
                const uint8_t *payload = ptr + 16;
                size_t payload_len = sz - 16;
                payload_out.assign(payload, payload + payload_len);
                return true;
            }
            cursor += sz;
        }
        return false;
    };

    size_t offset = 0;
    while (offset + 8 <= ilst.size()) {
        const uint8_t *ptr = ilst.data() + offset;
        uint32_t sz = be32(ptr);
        if (sz < 8 || offset + sz > ilst.size()) {
            break;
        }
        uint32_t typ = be32(ptr + 4);
        const uint8_t *item = ptr + 8;
        size_t item_len = sz - 8;

        std::vector<uint8_t> payload;
        uint32_t data_type = 0;
        if (extract_data_box(item, item_len, payload, data_type)) {
            auto set_string = [&](std::string &target) {
                target.assign(reinterpret_cast<const char *>(payload.data()), payload.size());
            };
            switch (typ) {
                case fourcc(0xa9, 'n', 'a', 'm'):  // ©nam
                    CH_LOG("debug", "ilst title bytes=" << payload.size());
                    set_string(out.title);
                    break;
                case fourcc(0xa9, 'A', 'R', 'T'):  // ©ART
                    CH_LOG("debug", "ilst artist bytes=" << payload.size());
                    set_string(out.artist);
                    break;
                case fourcc(0xa9, 'a', 'l', 'b'):  // ©alb
                    CH_LOG("debug", "ilst album bytes=" << payload.size());
                    set_string(out.album);
                    break;
                case fourcc(0xa9, 'g', 'e', 'n'):  // ©gen
                    CH_LOG("debug", "ilst genre bytes=" << payload.size());
                    set_string(out.genre);
                    break;
                case fourcc(0xa9, 'd', 'a', 'y'):  // ©day
                    CH_LOG("debug", "ilst year bytes=" << payload.size());
                    set_string(out.year);
                    break;
                case fourcc(0xa9, 'c', 'm', 't'):  // ©cmt
                    CH_LOG("debug", "ilst comment bytes=" << payload.size());
                    set_string(out.comment);
                    break;
                case fourcc('c', 'o', 'v', 'r'): {  // cover art
                    CH_LOG("debug", "ilst cover bytes=" << payload.size());
                    out.cover = std::move(payload);
                    break;
                }
                default:
                    break;
            }
        }
        offset += sz;
    }
}

struct PendingChapter {
    std::string title;
    uint32_t start_ms = 0;
    std::string image_path;
    std::string url;
    std::string url_text;
};

static bool load_chapters_json(
    const std::string &json_path, std::vector<ChapterTextSample> &texts,
    std::vector<ChapterImageSample> &images, MetadataSet &meta,
    std::vector<std::pair<std::string, std::vector<ChapterTextSample>>> &extra_text_tracks) {
    std::ifstream f(json_path);
    if (!f.is_open()) {
        CH_LOG("error", "open failed for " << json_path << " errno=" << errno << " ("
                                           << std::generic_category().message(errno) << ")");
        return false;
    }
    json j;
    f >> j;
    auto resolve_path = [&](const std::string &p) {
        auto base = std::filesystem::path(json_path).parent_path();
        return (p.empty() ? std::filesystem::path() : base / p).string();
    };
    std::vector<PendingChapter> pending;
    if (j.contains("chapters") && j["chapters"].is_array()) {
        pending.reserve(j["chapters"].size());
        for (auto &c : j["chapters"]) {
            PendingChapter p{};
            p.title = c.value("title", "");
            p.start_ms = c.value("start_ms", 0);
            p.image_path = c.value("image", "");
            p.url = c.value("url", "");
            p.url_text = c.value("url_text", "");
            pending.emplace_back(std::move(p));
        }
    }

    std::vector<ChapterTextSample> url_chapters;
    bool has_url_track = false;
    for (const auto &p : pending) {
        ChapterTextSample t{};
        t.text = p.title;
        t.start_ms = p.start_ms;
        // Mirror href onto the title track so AVFoundation surfaces it as an HREF extraAttribute.
        t.href = p.url;
        ChapterTextSample url_sample{};
        url_sample.start_ms = p.start_ms;
        // Default to empty URL text (Apple “golden” behavior); JSON may supply `url_text`.
        url_sample.text = p.url_text;
        if (!p.url.empty()) {
            has_url_track = true;
            url_sample.href = p.url;
        }
        if (!p.url_text.empty()) {
            has_url_track = true;
        }
        texts.push_back(t);
        url_chapters.emplace_back(std::move(url_sample));

        if (!p.image_path.empty()) {
            ChapterImageSample im{};
            im.data = load_jpeg(resolve_path(p.image_path));
            im.start_ms = t.start_ms;
            images.emplace_back(std::move(im));
        }
    }
    if (has_url_track) {
        extra_text_tracks.emplace_back("Chapter URLs", std::move(url_chapters));
    }
    meta.title = j.value("title", "");
    meta.artist = j.value("artist", "");
    meta.album = j.value("album", "");
    meta.genre = j.value("genre", "");
    meta.year = j.value("year", "");
    meta.comment = j.value("comment", "");
    std::string cover_path = j.value("cover", "");
    if (!cover_path.empty()) {
        meta.cover = load_jpeg(resolve_path(cover_path));
    }
    return true;
}

static bool metadata_is_empty(const MetadataSet &m) {
    return m.title.empty() && m.artist.empty() && m.album.empty() && m.genre.empty() &&
           m.year.empty() && m.comment.empty() && m.cover.empty();
}

}  // namespace

namespace chapterforge {

namespace {
Status make_status(bool ok, std::string msg = {}) { return Status{ok, std::move(msg)}; }
}  // namespace

Status mux_file_to_m4a(const std::string &input_audio_path,
                          const std::vector<ChapterTextSample> &text_chapters,
                          const std::vector<ChapterTextSample> &url_chapters,
                          const std::vector<ChapterImageSample> &image_chapters,
                          const MetadataSet &metadata, const std::string &output_path,
                          bool fast_start) {
    const auto t0 = std::chrono::steady_clock::now();
    CH_LOG("debug", "mux_file_to_m4a(titles+urls+images+meta) input=" << input_audio_path
                                                                      << " output=" << output_path
                                                                      << " fast_start=" << fast_start
                                                                      << " titles="
                                                                      << text_chapters.size()
                                                                      << " urls="
                                                                      << url_chapters.size()
                                                                      << " images="
                                                                      << image_chapters.size());
    auto aac = load_audio(input_audio_path);
    if (!aac) {
        std::string msg = "Failed to load audio from " + input_audio_path;
        CH_LOG("error", msg);
        return make_status(false, msg);
    }
    const auto t_load = std::chrono::steady_clock::now();
    Mp4aConfig cfg{};
    const std::vector<uint8_t> *ilst_ptr = nullptr;
    const std::vector<uint8_t> *meta_ptr = nullptr;
    const bool caller_has_meta = !metadata_is_empty(metadata);
    if (!caller_has_meta) {
        if (!aac->meta_payload.empty()) {
            meta_ptr = &aac->meta_payload;
            CH_LOG("debug", "Reusing source meta payload (" << meta_ptr->size() << " bytes)");
        }
        if (!aac->ilst_payload.empty()) {
            ilst_ptr = &aac->ilst_payload;
            CH_LOG("debug", "Reusing source ilst metadata (" << ilst_ptr->size() << " bytes)");
        } else {
            CH_LOG("warn",
                   "source metadata missing and no metadata provided; output will carry empty ilst");
        }
    } else {
        CH_LOG("debug", "Using metadata provided by caller (overrides source ilst/meta)");
    }
    std::vector<std::pair<std::string, std::vector<ChapterTextSample>>> extra_text_tracks;
    if (!url_chapters.empty()) {
        extra_text_tracks.push_back({"Chapter URLs", url_chapters});
    }
    bool ok = write_mp4(output_path, *aac, text_chapters, image_chapters, cfg, metadata,
                        fast_start, extra_text_tracks, ilst_ptr, meta_ptr);
    const auto t1 = std::chrono::steady_clock::now();
    const auto load_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t_load - t0).count();
    const auto mux_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t_load).count();
    const auto total_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    CH_LOG("debug", "mux_file_to_m4a(titles+urls+images+meta) timings ms: load=" << load_ms
                                                                                << " mux=" << mux_ms
                                                                                << " total="
                                                                                << total_ms);
    if (!ok) {
        return make_status(false, "Failed to write M4A to " + output_path);
    }
    return make_status(true);
}

Status mux_file_to_m4a(const std::string &input_audio_path,
                          const std::vector<ChapterTextSample> &text_chapters,
                          const std::vector<ChapterImageSample> &image_chapters,
                          const MetadataSet &metadata, const std::string &output_path,
                          bool fast_start) {
    std::vector<ChapterTextSample> empty_urls;
    return mux_file_to_m4a(input_audio_path, text_chapters, empty_urls, image_chapters, metadata,
                           output_path, fast_start);
}

Status mux_file_to_m4a(const std::string &input_audio_path,
                          const std::vector<ChapterTextSample> &text_chapters,
                          const MetadataSet &metadata, const std::string &output_path,
                          bool fast_start) {
    std::vector<ChapterImageSample> empty_images;
    std::vector<ChapterTextSample> empty_urls;
    return mux_file_to_m4a(input_audio_path, text_chapters, empty_urls, empty_images, metadata,
                           output_path, fast_start);
}

Status mux_file_to_m4a(const std::string &input_audio_path,
                          const std::vector<ChapterTextSample> &text_chapters,
                          const std::vector<ChapterImageSample> &image_chapters,
                          const std::string &output_path, bool fast_start) {
    MetadataSet empty{};
    std::vector<ChapterTextSample> empty_urls;
    return mux_file_to_m4a(input_audio_path, text_chapters, empty_urls, image_chapters, empty,
                           output_path, fast_start);
}

Status mux_file_to_m4a(const std::string &input_audio_path,
                          const std::vector<ChapterTextSample> &text_chapters,
                          const std::vector<ChapterTextSample> &url_chapters,
                          const std::vector<ChapterImageSample> &image_chapters,
                          const std::string &output_path,
                          bool fast_start) {
    MetadataSet empty{};
    return mux_file_to_m4a(input_audio_path, text_chapters, url_chapters, image_chapters, empty,
                           output_path, fast_start);
}

Status mux_file_to_m4a(const std::string &input_audio_path,
                          const std::string &chapter_json_path, const std::string &output_path,
                          bool fast_start) {
    CH_LOG("debug", "mux_file_to_m4a(json) input=" << input_audio_path
                                                   << " chapters=" << chapter_json_path
                                                   << " output=" << output_path
                                                   << " fast_start=" << fast_start);
    std::vector<ChapterTextSample> text_chapters;
    std::vector<ChapterImageSample> image_chapters;
    std::vector<std::pair<std::string, std::vector<ChapterTextSample>>> extra_text_tracks;
    MetadataSet meta;
    if (!load_chapters_json(chapter_json_path, text_chapters, image_chapters, meta,
                            extra_text_tracks)) {
        std::string msg = "Failed to load chapters JSON: " + chapter_json_path;
        CH_LOG("error", msg);
        return make_status(false, msg);
    }
    CH_LOG("debug", "chapters: titles=" << text_chapters.size()
                                        << " urls=" << extra_text_tracks.size()
                                        << " images=" << image_chapters.size());

    std::vector<ChapterTextSample> url_chapters;
    if (!extra_text_tracks.empty()) {
        url_chapters = std::move(extra_text_tracks.front().second);
    }
    return mux_file_to_m4a(input_audio_path, text_chapters, url_chapters, image_chapters, meta,
                           output_path, fast_start);
}

}  // namespace chapterforge

namespace {

uint32_t read_u32_be(const std::vector<uint8_t> &buf, size_t off) {
    return (static_cast<uint32_t>(buf[off]) << 24) | (static_cast<uint32_t>(buf[off + 1]) << 16) |
           (static_cast<uint32_t>(buf[off + 2]) << 8) | (static_cast<uint32_t>(buf[off + 3]));
}

uint16_t read_u16_be(const std::vector<uint8_t> &buf, size_t off) {
    return static_cast<uint16_t>((static_cast<uint16_t>(buf[off]) << 8) |
                                 static_cast<uint16_t>(buf[off + 1]));
}

using ::ChapterImageSample;
using ::ChapterTextSample;
using ::ParsedMp4;

struct SamplePlan {
    std::vector<uint64_t> offsets;
    std::vector<uint32_t> sizes;
};

std::optional<SamplePlan> build_sample_plan(const parser_detail::TrackParseResult &trk,
                                            std::istream &) {
    SamplePlan plan;
    // stsz: fixed or per-sample sizes
    const auto &stsz = trk.stsz;
    if (stsz.size() < 20) {
        return std::nullopt;
    }
    // stsz layout (payload only, size/type stripped):
    // [0..3]  version/flags
    // [4..7]  sample_size (0 means look at table)
    // [8..11] sample_count
    // [12..]  optional entry sizes
    uint32_t sample_size = read_u32_be(stsz, 4);
    uint32_t sample_count = read_u32_be(stsz, 8);
    if (sample_size == 0) {
        if (stsz.size() < 12 + sample_count * 4) {
            return std::nullopt;
        }
        plan.sizes.reserve(sample_count);
        for (uint32_t i = 0; i < sample_count; ++i) {
            plan.sizes.push_back(read_u32_be(stsz, 12 + i * 4));
        }
    } else {
        plan.sizes.assign(sample_count, sample_size);
    }

    // stco: chunk offsets
    const auto &stco = trk.stco;
    if (stco.size() < 16) {
        return std::nullopt;
    }
    uint32_t chunk_count = read_u32_be(stco, 4);
    if (stco.size() < 8 + chunk_count * 4) {
        return std::nullopt;
    }
    std::vector<uint64_t> chunk_offsets;
    chunk_offsets.reserve(chunk_count);
    for (uint32_t i = 0; i < chunk_count; ++i) {
        chunk_offsets.push_back(read_u32_be(stco, 8 + i * 4));
    }

    // stsc: samples per chunk mapping
    const auto &stsc = trk.stsc;
    if (stsc.size() < 16) {
        return std::nullopt;
    }
    uint32_t entry_count = read_u32_be(stsc, 4);
    if (stsc.size() < 8 + entry_count * 12) {
        return std::nullopt;
    }
    struct StscEntry {
        uint32_t first_chunk;
        uint32_t samples_per_chunk;
    };
    std::vector<StscEntry> entries;
    entries.reserve(entry_count);
    for (uint32_t i = 0; i < entry_count; ++i) {
        size_t off = 8 + i * 12;
        uint32_t first_chunk = read_u32_be(stsc, off);
        uint32_t samples_per_chunk = read_u32_be(stsc, off + 4);
        entries.push_back({first_chunk, samples_per_chunk});
    }

    plan.offsets.reserve(plan.sizes.size());
    size_t sample_index = 0;
    for (uint32_t chunk_idx = 1; chunk_idx <= chunk_count && sample_index < plan.sizes.size();
         ++chunk_idx) {
        // Find current stsc entry
        uint32_t samples_per_chunk = entries.back().samples_per_chunk;
        for (size_t e = 0; e + 1 < entries.size(); ++e) {
            if (chunk_idx >= entries[e].first_chunk && chunk_idx < entries[e + 1].first_chunk) {
                samples_per_chunk = entries[e].samples_per_chunk;
                break;
            }
        }
        uint64_t base_offset = chunk_offsets[chunk_idx - 1];
        uint64_t cursor = base_offset;
        for (uint32_t s = 0; s < samples_per_chunk && sample_index < plan.sizes.size(); ++s) {
            plan.offsets.push_back(cursor);
            cursor += plan.sizes[sample_index];
            ++sample_index;
        }
    }
    if (plan.offsets.size() != plan.sizes.size()) {
        return std::nullopt;
    }
    return plan;
}

std::vector<uint32_t> build_start_times_ms(const parser_detail::TrackParseResult &trk) {
    std::vector<uint32_t> starts;
    if (trk.timescale == 0 || trk.stts.empty()) {
        return starts;
    }
    // stts (decoding time-to-sample) box:
    // [0..3]=version/flags, [4..7]=entry_count, followed by entry_count pairs
    // of (sample_count, sample_delta).
    uint32_t entry_count = read_u32_be(trk.stts, 4);
    if (trk.stts.size() < 8 + entry_count * 8) {
        return starts;
    }
    uint64_t cum = 0;
    starts.reserve(trk.sample_count);
    for (uint32_t i = 0; i < entry_count; ++i) {
        size_t off = 8 + i * 8;
        uint32_t count = read_u32_be(trk.stts, off);
        uint32_t delta = read_u32_be(trk.stts, off + 4);
        for (uint32_t j = 0; j < count; ++j) {
            uint32_t ms = static_cast<uint32_t>((cum * 1000) / trk.timescale);
            starts.push_back(ms);
            cum += delta;
        }
    }
    return starts;
}

struct ExtractedTracks {
    std::vector<ChapterTextSample> titles;
    std::vector<ChapterTextSample> urls;
    std::vector<ChapterImageSample> images;
};

bool is_url_track_name(const std::string &name) {
    std::string lower = name;
    for (auto &c : lower) {
        c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    }
    return lower.find("url") != std::string::npos;
}

std::vector<ChapterTextSample> parse_tx3g_track(const parser_detail::TrackParseResult &trk,
                                                std::istream &in) {
    std::vector<ChapterTextSample> out;
    auto starts = build_start_times_ms(trk);
    auto plan_opt = build_sample_plan(trk, in);
    if (!plan_opt) {
        return out;
    }
    const auto &plan = *plan_opt;
    size_t sample_count = std::min({plan.offsets.size(), plan.sizes.size(), starts.size()});
    CH_LOG("debug", "tx3g track: plan sizes=" << plan.sizes.size()
                                              << " offsets=" << plan.offsets.size()
                                              << " starts=" << starts.size()
                                              << " sample_count=" << sample_count
                                              << " size[0]="
                                              << (plan.sizes.empty() ? 0 : plan.sizes[0]));
    out.reserve(sample_count);
    for (size_t i = 0; i < sample_count; ++i) {
        uint64_t off = plan.offsets[i];
        uint32_t sz = plan.sizes[i];
        if (sz < 2) {
            continue;
        }
        std::vector<uint8_t> buf(sz);
        in.seekg(static_cast<std::streamoff>(off), std::ios::beg);
        in.read(reinterpret_cast<char *>(buf.data()), sz);
        uint16_t text_len = read_u16_be(buf, 0);
        size_t text_bytes = std::min<size_t>(text_len, sz > 2 ? sz - 2 : 0);
        ChapterTextSample chapter_sample{};
        chapter_sample.start_ms = starts[i];
        chapter_sample.text.assign(reinterpret_cast<const char *>(buf.data() + 2), text_bytes);
        size_t cursor = 2 + text_bytes;
        // Optional href box
        while (cursor + 8 <= sz) {
            uint32_t box_size = read_u32_be(buf, cursor);
            uint32_t box_type = read_u32_be(buf, cursor + 4);
            if (box_size < 8 || cursor + box_size > sz) {
                break;
            }
            if (box_type == 0x68726566) {  // 'href'
                if (box_size >= 8 + 2 + 2 + 1) {
                    uint8_t url_len = buf[cursor + 8 + 2 + 2];
                    size_t url_off = cursor + 8 + 2 + 2 + 1;
                    if (url_off + url_len <= cursor + box_size) {
                        chapter_sample.href.assign(
                            reinterpret_cast<const char *>(buf.data() + url_off), url_len);
                    }
                }
            }
            cursor += box_size;
        }
        out.push_back(std::move(chapter_sample));
    }
    return out;
}

std::vector<ChapterImageSample> parse_image_track(const parser_detail::TrackParseResult &trk,
                                                  std::istream &in) {
    std::vector<ChapterImageSample> out;
    auto starts = build_start_times_ms(trk);
    auto plan_opt = build_sample_plan(trk, in);
    if (!plan_opt) {
        return out;
    }
    const auto &plan = *plan_opt;
    size_t sample_count = std::min({plan.offsets.size(), plan.sizes.size(), starts.size()});
    out.reserve(sample_count);
    for (size_t i = 0; i < sample_count; ++i) {
        uint64_t off = plan.offsets[i];
        uint32_t sz = plan.sizes[i];
        if (sz == 0) {
            continue;
        }
        std::vector<uint8_t> buf(sz);
        in.seekg(static_cast<std::streamoff>(off), std::ios::beg);
        in.read(reinterpret_cast<char *>(buf.data()), sz);
        ChapterImageSample img_sample{};
        img_sample.start_ms = starts[i];
        img_sample.data = std::move(buf);
        out.push_back(std::move(img_sample));
    }
    return out;
}

ExtractedTracks extract_tracks(const ParsedMp4 &parsed, std::istream &in) {
    ExtractedTracks ext;
    const parser_detail::TrackParseResult *titles = nullptr;
    const parser_detail::TrackParseResult *urls = nullptr;
    const parser_detail::TrackParseResult *images = nullptr;
    std::vector<const parser_detail::TrackParseResult *> text_tracks;

    for (const auto &trk : parsed.tracks) {
        if (trk.handler_type == 0x74657874) {  // 'text'
            text_tracks.push_back(&trk);
            if (!urls && is_url_track_name(trk.handler_name)) {
                urls = &trk;
            } else if (!titles) {
                titles = &trk;
            }
        } else if (trk.handler_type == 0x76696465) {  // 'vide'
            if (!images) {
                images = &trk;
            }
        }
    }

    // If we did not conclusively identify a URL track by name, but there are multiple
    // text tracks, treat the second one as URLs (mirrors how we author files).
    if (!urls && text_tracks.size() > 1) {
        urls = text_tracks[1];
    }

    if (titles) {
        ext.titles = parse_tx3g_track(*titles, in);
    }
    if (urls) {
        ext.urls = parse_tx3g_track(*urls, in);
    }
    if (images) {
        ext.images = parse_image_track(*images, in);
    }
    return ext;
}

}  // namespace

namespace chapterforge {

ReadResult read_m4a(const std::string &path) {
    ReadResult result{};
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        result.status = {false, "Failed to open " + path};
        return result;
    }
    auto parsed = parse_mp4(path);
    if (!parsed) {
        result.status = {false, "Failed to parse " + path};
        return result;
    }
    auto ext = extract_tracks(*parsed, in);
    result.titles = std::move(ext.titles);
    result.urls = std::move(ext.urls);
    result.images = std::move(ext.images);
    if (!parsed->ilst_payload.empty()) {
        parse_ilst_metadata(parsed->ilst_payload, result.metadata);
        CH_LOG("debug", "parsed metadata title='" << result.metadata.title << "' artist='"
                                                  << result.metadata.artist << "' album='"
                                                  << result.metadata.album << "' genre='"
                                                  << result.metadata.genre << "' year='"
                                                  << result.metadata.year << "' comment='"
                                                  << result.metadata.comment
                                                  << "' cover_bytes=" << result.metadata.cover.size());
    }
    result.status = {true, ""};
    return result;
}

}  // namespace chapterforge
