//
//  trak_builder.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//

#pragma once

#include "mp4_atoms.hpp"
#include <cstdint>
#include <memory>
#include <vector>

// Build full audio track (with tref linking to chapter tracks)
std::unique_ptr<Atom>
build_trak_audio(uint32_t track_id, uint32_t timescale, uint64_t duration_ts,
                 std::unique_ptr<Atom> stbl_audio,
                 const std::vector<uint32_t> &chapter_ref_track_ids,
                 uint64_t tkhd_duration_mvhd);

// Build full text chapter track
std::unique_ptr<Atom> build_trak_text(uint32_t track_id, uint32_t timescale,
                                      uint64_t duration_ts,
                                      std::unique_ptr<Atom> stbl_text,
                                      uint64_t tkhd_duration_mvhd);

// Build full image chapter track
std::unique_ptr<Atom> build_trak_image(uint32_t track_id, uint32_t timescale,
                                       uint64_t duration_ts,
                                       std::unique_ptr<Atom> stbl_image,
                                       uint16_t width, uint16_t height,
                                       uint64_t tkhd_duration_mvhd);
