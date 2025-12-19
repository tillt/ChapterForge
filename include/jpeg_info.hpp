//
//  jpeg_info.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//
#pragma once

#include <cstdint>
#include <vector>

// Minimal JPEG header inspection helper used to obtain dimensions and subsampling.
// Returns true when width/height were found; sets is_yuv420 when sampling factors match 4:2:0.
bool parse_jpeg_info(const std::vector<uint8_t> &data, uint16_t &width, uint16_t &height,
                     bool &is_yuv420);

