//
//  tkhd_builder.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#pragma once
#include <memory>

#include "mp4_atoms.hpp"

std::unique_ptr<Atom> build_tkhd_audio(uint32_t track_id, uint64_t duration);
std::unique_ptr<Atom> build_tkhd_text(uint32_t track_id, uint64_t duration, bool enabled);
std::unique_ptr<Atom> build_tkhd_image(uint32_t track_id, uint64_t duration, uint16_t width,
                                       uint16_t height);
