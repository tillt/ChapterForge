//
//  mdia_builder.hpp
//  mp4chapters
//
//  Created by Till Toenshoff on 12/9/25.
//

#pragma once
#include "mp4_atoms.hpp"
#include <memory>

// Build mdia for audio/text/image
std::unique_ptr<Atom> build_mdia_audio(uint32_t timescale, uint64_t duration_ts,
                                       std::unique_ptr<Atom> stbl);

std::unique_ptr<Atom> build_mdia_text(uint32_t timescale, uint64_t duration_ts,
                                      std::unique_ptr<Atom> stbl);

std::unique_ptr<Atom> build_mdia_image(uint32_t timescale, uint64_t duration_ts,
                                       std::unique_ptr<Atom> stbl);
