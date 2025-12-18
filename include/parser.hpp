//
//  parser.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#pragma once

#include <cstdint>
#include <istream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct Mp4AtomInfo {
    uint32_t type;
    uint64_t size;    // total atom size.
    uint64_t offset;  // offset in file.
};

// Minimal parsed MP4 data for our authoring needs.
struct ParsedMp4 {
    bool used_fallback_stbl = false;  // true if stbl atoms were recovered via flat scan.

    // ilst metadata atom payload (optional).
    std::vector<uint8_t> ilst_payload;
    // raw meta payload (version/flags/reserved + children) to allow verbatim reuse.
    std::vector<uint8_t> meta_payload;

    // audio track timing info.
    uint32_t audio_timescale = 0;
    uint64_t audio_duration = 0;

    // sample table (copied directly)
    std::vector<uint8_t> stsd;
    std::vector<uint8_t> stts;
    std::vector<uint8_t> stsc;
    std::vector<uint8_t> stsz;
    std::vector<uint8_t> stco;
};

// Utility: read big-endian 32-bit value.
uint32_t read_u32(std::istream &in);

// Utility: read big-endian 64-bit value.
uint64_t read_u64(std::istream &in);

// Main parsing entry point.
std::optional<ParsedMp4> parse_mp4(const std::string &path);

namespace parser_detail {
struct TrackParseResult {
    uint32_t handler_type = 0;
    uint32_t timescale = 0;
    uint64_t duration = 0;
    uint32_t sample_count = 0;
    std::vector<uint8_t> stsd;
    std::vector<uint8_t> stts;
    std::vector<uint8_t> stsc;
    std::vector<uint8_t> stsz;
    std::vector<uint8_t> stco;
};
}  // namespace parser_detail

#ifdef CHAPTERFORGE_TESTING
// Test-only wrappers that allow unit tests to exercise lower-level parsing.
std::optional<parser_detail::TrackParseResult> parse_trak_for_test(std::istream &in,
                                                                   uint64_t payload_size,
                                                                   uint64_t file_size,
                                                                   bool &force_fallback);
void parse_moov_for_test(std::istream &in, const Mp4AtomInfo &atom, uint64_t file_size,
                         ParsedMp4 &out, uint32_t &best_audio_samples, bool &force_fallback);
#endif
