//
//  aac_extractor.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct AacExtractResult {
    std::vector<std::vector<uint8_t>> frames;  // raw AAC frames (ADTS header stripped)
    std::vector<uint32_t> sizes;               // raw frame sizes

    uint32_t sample_rate = 0;
    uint8_t sampling_index = 0;
    uint8_t channel_config = 0;
    uint8_t audio_object_type = 0;  // e.g. 2 = AAC LC

    // Optional: original stsd payload (for mp4 inputs) so we can reuse esds.
    std::vector<uint8_t> stsd_payload;
    std::vector<uint8_t> stts_payload;
    std::vector<uint8_t> stsc_payload;
    std::vector<uint8_t> stsz_payload;
    std::vector<uint8_t> stco_payload;

    // Optional: original ilst payload (when source is MP4/M4A)
    std::vector<uint8_t> ilst_payload;
};

AacExtractResult extract_adts_frames(const std::vector<uint8_t> &data);

// Extract AAC frames and related tables from an MP4/M4A source (preferred.
// when the input is already an MP4 container)
std::optional<AacExtractResult> extract_from_mp4(const std::string &path);
