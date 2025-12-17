//
//  aac_extractor.cpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#include "aac_extractor.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <optional>
#include <cstring>

#include "logging.hpp"
#include "mp4_atoms.hpp"
#include "mp4a_builder.hpp"
#include "parser.hpp"

namespace {

constexpr uint8_t kAdtsSyncByte = 0xFF;
constexpr uint8_t kAdtsSyncMask = 0xF0;
constexpr uint8_t kAdtsSyncPattern = 0xF0;
constexpr size_t kAdtsHeaderNoCrc = 7;
constexpr size_t kAdtsHeaderWithCrc = 9;
constexpr size_t kEsdsVersionFlagsSize = 4;
constexpr size_t kStszHeaderSize = 12;
constexpr size_t kStscHeaderSize = 8;
constexpr size_t kStscEntrySize = 12;
constexpr size_t kEsdsTagLengthFieldMaxBytes = 4;

}  // namespace

AacExtractResult extract_adts_frames(const std::vector<uint8_t> &data) {
    AacExtractResult out;

    size_t i = 0;
    while (i + 7 < data.size()) {
        if (data[i] == kAdtsSyncByte && (data[i + 1] & kAdtsSyncMask) == kAdtsSyncPattern) {
            uint32_t len =
                ((data[i + 3] & 0x03) << 11) | (data[i + 4] << 3) | ((data[i + 5] & 0xE0) >> 5);

            if (len < 7 || i + len > data.size()) {
                i++;
                continue;
            }

            // Parse header for config on first frame.
            if (out.frames.empty()) {
                uint8_t profile = (data[i + 2] >> 6) & 0x03;
                out.audio_object_type = profile + 1;  // 1=MAIN,2=LC,...
                out.sampling_index = (data[i + 2] >> 2) & 0x0F;
                out.channel_config = ((data[i + 2] & 0x01) << 2) | ((data[i + 3] >> 6) & 0x03);

                static const uint32_t sf_table[16] = {96000, 88200, 64000, 48000, 44100, 32000,
                                                      24000, 22050, 16000, 12000, 11025, 8000,
                                                      7350,  0,     0,     0};
                out.sample_rate = out.sampling_index < 16 ? sf_table[out.sampling_index] : 0;
            }

            // strip ADTS header (7 or 9 bytes depending on CRC)
            bool protection_absent = (data[i + 1] & 0x01);
            size_t header_size = protection_absent ? kAdtsHeaderNoCrc : kAdtsHeaderWithCrc;
            if (len <= header_size) {
                i++;
                continue;
            }

            size_t payload_len = len - header_size;
            std::vector<uint8_t> frame(payload_len);
            std::copy_n(data.begin() + i + header_size, payload_len, frame.begin());

            out.frames.emplace_back(std::move(frame));
            out.sizes.push_back(static_cast<uint32_t>(payload_len));

            i += len;
        } else {
            i++;
        }
    }

    return out;
}

// Helpers for MP4 extraction (from container)
static std::optional<std::vector<uint32_t>> parse_stsz_sizes(
    const std::vector<uint8_t> &stsz_payload) {
    if (stsz_payload.size() < kStszHeaderSize) {
        return std::nullopt;
    }
    const uint8_t *p = stsz_payload.data();
    uint32_t sample_size = (p[4] << 24) | (p[5] << 16) | (p[6] << 8) | p[7];
    uint32_t sample_count = (p[8] << 24) | (p[9] << 16) | (p[10] << 8) | p[11];
    std::vector<uint32_t> sizes;
    if (sample_size != 0) {
        sizes.assign(sample_count, sample_size);
        return sizes;
    }
    if (stsz_payload.size() < kStszHeaderSize + sample_count * 4) {
        return std::nullopt;
    }
    sizes.reserve(sample_count);
    size_t pos = kStszHeaderSize;
    for (uint32_t i = 0; i < sample_count; ++i) {
        uint32_t s = (p[pos] << 24) | (p[pos + 1] << 16) | (p[pos + 2] << 8) | p[pos + 3];
        sizes.push_back(s);
        pos += 4;
    }
    return sizes;
}

static void parse_esds_audio_cfg(const std::vector<uint8_t> &stsd_payload, Mp4aConfig &cfg) {
    for (size_t i = 0; i + 8 <= stsd_payload.size();) {
        uint32_t size = (stsd_payload[i] << 24) | (stsd_payload[i + 1] << 16) |
                        (stsd_payload[i + 2] << 8) | stsd_payload[i + 3];
        uint32_t type = (stsd_payload[i + 4] << 24) | (stsd_payload[i + 5] << 16) |
                        (stsd_payload[i + 6] << 8) | stsd_payload[i + 7];
        if (size < 8) {
            break;
        }
        if (type == fourcc("esds")) {
            size_t pos = i + 8 + kEsdsVersionFlagsSize;  // skip version/flags
            while (pos + 2 < stsd_payload.size()) {
                uint8_t tag = stsd_payload[pos++];
                uint32_t len = 0;
                for (int j = 0; j < static_cast<int>(kEsdsTagLengthFieldMaxBytes) &&
                                pos < stsd_payload.size();
                     ++j) {
                    uint8_t b = stsd_payload[pos++];
                    len = (len << 7) | (b & 0x7F);
                    if ((b & 0x80) == 0) {
                        break;
                    }
                }
                if (tag == 0x05 && pos + len <= stsd_payload.size() && len >= 2) {
                    uint8_t b0 = stsd_payload[pos];
                    uint8_t b1 = stsd_payload[pos + 1];
                    uint8_t aot = (b0 >> 3) & 0x1F;
                    uint8_t sf_idx = ((b0 & 0x07) << 1) | ((b1 >> 7) & 0x01);
                    uint8_t ch_cfg = (b1 >> 3) & 0x0F;
                    cfg.audio_object_type = aot;
                    cfg.sampling_index = sf_idx;
                    cfg.channel_config = ch_cfg;
                    static const uint32_t sf_table[16] = {96000, 88200, 64000, 48000, 44100, 32000,
                                                          24000, 22050, 16000, 12000, 11025, 8000,
                                                          7350,  0,     0,     0};
                    if (sf_idx < 16) {
                        cfg.sample_rate = sf_table[sf_idx];
                    }
                    break;
                }
                pos += len;
            }
            break;
        }
        i += size;
    }
}

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
        uint32_t chunk_count = (next_first > 0) ? (next_first - first_chunk) : 0;
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
    return plan;
}

std::optional<AacExtractResult> extract_from_mp4(const std::string &path) {
    const auto t0 = std::chrono::steady_clock::now();
    CH_LOG("debug", "mp4 reuse start: " << path);
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        CH_LOG("debug", "Failed to open MP4: " << path << " (errno=" << errno
                                               << " msg=" << std::strerror(errno) << ")");
        return std::nullopt;
    }

    // Measure file size for safety bounds.
    f.seekg(0, std::ios::end);
    const auto file_size = static_cast<uint64_t>(f.tellg());
    f.seekg(0, std::ios::beg);
    if (file_size == 0) {
        CH_LOG("error", "Empty or unreadable MP4: " << path);
        return std::nullopt;
    }
    const auto t_open = std::chrono::steady_clock::now();
    CH_LOG("debug", "calling parse_mp4 size=" << file_size << " path=" << path);
    auto parsed_opt = parse_mp4(path);
    if (!parsed_opt) {
        CH_LOG("error", "Failed to parse MP4 (required moov/stbl atoms not found): " << path);
        return std::nullopt;
    }
    CH_LOG("debug", "mp4 parsed optional has value for " << path);
    ParsedMp4 &parsed = *parsed_opt;
    CH_LOG("debug", "mp4 parsed: stco=" << parsed.stco.size() << " stsc=" << parsed.stsc.size()
                                        << " stsz=" << parsed.stsz.size()
                                        << " stsd=" << parsed.stsd.size());
    if (parsed.stco.empty() || parsed.stsc.empty() || parsed.stsz.empty() || parsed.stsd.empty()) {
        CH_LOG("error", "Missing required stbl atoms (stco/stsc/stsz/stsd) in " << path);
        return std::nullopt;
    }

    auto sizes_opt = parse_stsz_sizes(parsed.stsz);
    if (!sizes_opt || parsed.stco.empty() || parsed.stsc.empty()) {
        return std::nullopt;
    }
    const auto &sizes = *sizes_opt;
    if (sizes.empty()) {
        return std::nullopt;
    }
    // Sanity: bound sample count to reasonable size relative to file.
    if (sizes.size() > file_size / 8) {  // heuristic: avg sample >= 8 bytes
        CH_LOG("error", "Unreasonable sample count (" << sizes.size()
                                                      << "); aborting parse for " << path);
        return std::nullopt;
    }

    CH_LOG("debug", "mp4 reuse: sizes=" << sizes.size() << " stco_bytes=" << parsed.stco.size()
                                        << " stsc_bytes=" << parsed.stsc.size()
                                        << " file_size=" << file_size);

    std::vector<uint32_t> chunk_plan =
        derive_chunk_plan(parsed.stsc, static_cast<uint32_t>(sizes.size()));
    if (chunk_plan.empty()) {
        return std::nullopt;
    }
    if (chunk_plan.size() > sizes.size() * 4 || chunk_plan.size() > 1000000) {
        CH_LOG("error", "Unreasonable chunk plan size=" << chunk_plan.size()
                                                        << " samples=" << sizes.size());
        return std::nullopt;
    }

    std::vector<std::vector<uint8_t>> frames;
    frames.reserve(sizes.size());
    size_t sample_idx = 0;
    const uint8_t *pco = parsed.stco.data();
    uint32_t stco_count = (pco[4] << 24) | (pco[5] << 16) | (pco[6] << 8) | pco[7];
    // Validate stco table size matches count and fits in file.
    uint64_t stco_expected = 8ull + 4ull * stco_count;
    if (parsed.stco.size() < stco_expected || stco_expected > file_size) {
        CH_LOG("error", "stco table truncated: size=" << parsed.stco.size()
                                                      << " expected>=" << stco_expected);
        return std::nullopt;
    }

    // Validate stsc table size matches entry count (12 bytes each after header).
    if (parsed.stsc.size() < 8) {
        return std::nullopt;
    }
    uint32_t stsc_entries =
        (parsed.stsc[4] << 24) | (parsed.stsc[5] << 16) | (parsed.stsc[6] << 8) | parsed.stsc[7];
    uint64_t stsc_expected = 8ull + 12ull * stsc_entries;
    if (parsed.stsc.size() < stsc_expected || stsc_expected > file_size) {
        CH_LOG("error", "stsc table truncated: size=" << parsed.stsc.size()
                                                      << " expected>=" << stsc_expected);
        return std::nullopt;
    }
    CH_LOG("debug", "mp4 reuse: stco_count=" << stco_count << " stsc_entries=" << stsc_entries
                                             << " chunk_plan=" << chunk_plan.size());

    const auto t_parse = std::chrono::steady_clock::now();

    size_t stco_pos = 8;
    for (uint32_t chunk_idx = 0; chunk_idx < stco_count && chunk_idx < chunk_plan.size() &&
                                 sample_idx < sizes.size();
         ++chunk_idx) {
        if (stco_pos + 4 > parsed.stco.size()) {
            break;
        }
        uint32_t chunk_offset = (pco[stco_pos] << 24) | (pco[stco_pos + 1] << 16) |
                                (pco[stco_pos + 2] << 8) | pco[stco_pos + 3];
        stco_pos += 4;
        uint32_t samples_in_chunk = chunk_plan[chunk_idx];
        uint64_t chunk_size = 0;
        for (uint32_t i = 0; i < samples_in_chunk && sample_idx + i < sizes.size(); ++i) {
            chunk_size += sizes[sample_idx + i];
        }
        if (chunk_size == 0) {
            break;
        }

        // Bounds guard: chunk must fit in file.
        if (static_cast<uint64_t>(chunk_offset) + chunk_size > file_size) {
            CH_LOG("error", "Chunk exceeds file size: offset=" << chunk_offset
                                                               << " size=" << chunk_size
                                                               << " file_size=" << file_size);
            break;
        }

        std::vector<uint8_t> chunk(chunk_size);
        f.seekg(static_cast<std::streamoff>(chunk_offset), std::ios::beg);
        f.read(reinterpret_cast<char *>(chunk.data()), static_cast<std::streamsize>(chunk_size));
        if (f.gcount() != static_cast<std::streamsize>(chunk_size)) {
            break;
        }
        size_t offset = 0;
        for (uint32_t i = 0; i < samples_in_chunk && sample_idx < sizes.size(); ++i, ++sample_idx) {
            uint32_t s = sizes[sample_idx];
            if (offset + s > chunk.size()) {
                CH_LOG("error", "sample range exceeds chunk: offset=" << offset
                                                                      << " size=" << s
                                                                      << " chunk=" << chunk.size());
                break;
            }
            frames.emplace_back(chunk.begin() + offset, chunk.begin() + offset + s);
            offset += s;
        }
    }
    const auto t_samples = std::chrono::steady_clock::now();

    if (frames.size() != sizes.size()) {
        return std::nullopt;
    }

    AacExtractResult out;
    out.frames = std::move(frames);
    out.sizes = sizes;
    out.sample_rate = parsed.audio_timescale;
    Mp4aConfig cfg;
    cfg.sample_rate = parsed.audio_timescale;
    parse_esds_audio_cfg(parsed.stsd, cfg);
    if (cfg.sample_rate) {
        out.sample_rate = cfg.sample_rate;
    }
    out.channel_config = cfg.channel_count;
    out.sampling_index = cfg.sampling_index;
    out.audio_object_type = cfg.audio_object_type;
    out.stsd_payload = parsed.stsd;
    out.stts_payload = parsed.stts;
    out.stsc_payload = parsed.stsc;
    out.stsz_payload = parsed.stsz;
    out.stco_payload = parsed.stco;
    out.meta_payload = parsed.meta_payload;
    out.ilst_payload = parsed.ilst_payload;
    const auto t_done = std::chrono::steady_clock::now();
    const auto open_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t_open - t0).count();
    const auto parse_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t_parse - t_open).count();
    const auto samples_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t_samples - t_parse).count();
    const auto total_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t_done - t0).count();
    CH_LOG("debug", "mp4 reuse timings ms: open=" << open_ms << " parse=" << parse_ms
                                                  << " samples=" << samples_ms
                                                  << " total=" << total_ms);
    return out;
}
