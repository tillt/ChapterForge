//
//  stbl_image_builder.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//

#pragma once
#include "mp4_atoms.hpp"
#include <memory>
#include <string>
#include <vector>

struct ChapterImageSample {
  std::vector<uint8_t> data; // JPEG binary data
  uint32_t duration_ms;
};

std::unique_ptr<Atom>
build_image_stbl(const std::vector<ChapterImageSample> &samples,
                 uint32_t track_timescale, uint16_t width, uint16_t height,
                 const std::vector<uint32_t> &chunk_plan);
