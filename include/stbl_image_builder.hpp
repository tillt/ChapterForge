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

#include "chapter_image_sample.hpp"
#include "mp4_atoms.hpp"

std::unique_ptr<Atom> build_image_stbl(const std::vector<ChapterImageSample> &samples,
                                       uint32_t track_timescale, uint16_t width, uint16_t height,
                                       const std::vector<uint32_t> &chunk_plan,
                                       uint32_t total_ms = 0);
