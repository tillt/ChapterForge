//
//  stbl_builder.hpp.
//  ChapterForge.
//
//  Created by Till Toenshoff on 12/9/25.
//

#pragma once
#include <cstdint>
#include <memory>
#include <vector>

#include "mp4_atoms.hpp"

// Builds a complete stbl for a text chapter track.
std::unique_ptr<Atom> build_text_stbl(const std::vector<uint32_t> &timestamps_ts,
                                      uint32_t total_duration_ts,
                                      const std::vector<uint32_t> &sample_sizes);

// Builds a complete stbl for an image chapter track.
std::unique_ptr<Atom> build_image_stbl(const std::vector<uint32_t> &timestamps_ts,
                                       uint32_t total_duration_ts,
                                       const std::vector<uint32_t> &sample_sizes, uint16_t width,
                                       uint16_t height);
