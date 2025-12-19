//
//  chapter_image_sample.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#pragma once

#include <cstdint>
#include <vector>

/// @ingroup api
/// Represents a chapter image sample (JPEG payload and start time).
struct ChapterImageSample {
    std::vector<uint8_t> data;  ///< Raw JPEG bytes (must be YUVJ420)
    uint32_t start_ms = 0;      ///< Absolute start time in ms
};
