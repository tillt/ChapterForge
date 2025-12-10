//
//  mp4a_builder.hpp.
//  ChapterForge.
//
//  Created by Till Toenshoff on 12/9/25.
//

#pragma once
#include <memory>
#include <string>
#include <vector>

#include "mp4_atoms.hpp"

// Audio specific config for ESDS.
struct Mp4aConfig {
    uint16_t channel_count = 2;
    uint16_t sample_size = 16;
    uint32_t sample_rate = 44100;

    uint8_t audio_object_type = 2;  // AAC LC = 2
    uint8_t sampling_index = 4;     // 44100Hz = index 4 in MPEG table
    uint8_t channel_config = 2;     // 2 = stereo
};

std::unique_ptr<Atom> build_mp4a(const Mp4aConfig &cfg);
