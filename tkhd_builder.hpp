//
//  tkhd_builder.hpp
//  mp4chapters
//
//  Created by Till Toenshoff on 12/9/25.
//

#pragma once
#include "mp4_atoms.hpp"
#include <memory>

std::unique_ptr<Atom> build_tkhd_audio(uint32_t track_id, uint64_t duration);
std::unique_ptr<Atom> build_tkhd_text(uint32_t track_id, uint64_t duration);
std::unique_ptr<Atom> build_tkhd_image(uint32_t track_id, uint64_t duration,
                                       uint16_t width, uint16_t height);
