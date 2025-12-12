//
//  stbl_metadata_builder.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "mp4_atoms.hpp"

// Represents a timed metadata sample aligned to chapter timing.
struct ChapterMetadataSample {
    std::string payload;   // UTF-8 content
    uint32_t start_ms = 0; // absolute start time in ms
};

std::unique_ptr<Atom> build_metadata_stbl(const std::vector<ChapterMetadataSample> &samples,
                                          uint32_t track_timescale);
