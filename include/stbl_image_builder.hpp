//
//  stbl_image_builder.hpp
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

/// Represents a chapter image sample (JPEG).
/// - `data`: Raw JPEG bytes for the chapter thumbnail/frame.
/// - `start_ms`: Absolute start time in milliseconds.
struct ChapterImageSample {
    std::vector<uint8_t> data;  // JPEG binary data
    uint32_t start_ms = 0;      // absolute start time in ms
};

std::unique_ptr<Atom> build_image_stbl(const std::vector<ChapterImageSample> &samples,
                                       uint32_t track_timescale, uint16_t width, uint16_t height,
                                       const std::vector<uint32_t> &chunk_plan,
                                       uint32_t total_ms = 0);
