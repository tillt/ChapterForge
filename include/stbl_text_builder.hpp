//
//  stbl_text_builder.hpp.
//  ChapterForge.
//
//  Created by Till Toenshoff on 12/9/25.
//

#pragma once
#include <memory>
#include <string>
#include <vector>

#include "mp4_atoms.hpp"

// Represents a chapter title sample.
struct ChapterTextSample {
    std::string text;       // UTF-8 text
    std::string href;       // optional hyperlink URL (tx3g modifier)
    uint32_t start_ms = 0;  // absolute start time in ms
};

std::unique_ptr<Atom> build_text_stbl(const std::vector<ChapterTextSample> &samples,
                                      uint32_t track_timescale);
