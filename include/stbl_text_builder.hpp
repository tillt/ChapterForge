//
//  stbl_text_builder.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//

#pragma once
#include "mp4_atoms.hpp"
#include <memory>
#include <string>
#include <vector>

// Represents a chapter title sample
struct ChapterTextSample {
  std::string text;     // UTF-8 text
  uint32_t duration_ms; // duration in ms
};

std::unique_ptr<Atom>
build_text_stbl(const std::vector<ChapterTextSample> &samples,
                uint32_t track_timescale);
