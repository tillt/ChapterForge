//
//  moov_builder.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//

#pragma once
#include "mp4_atoms.hpp"
#include <memory>

std::unique_ptr<Atom> build_moov(uint32_t timescale, uint64_t duration_ts,
                                 std::unique_ptr<Atom> trak_audio,
                                 std::unique_ptr<Atom> trak_text,
                                 std::unique_ptr<Atom> trak_image,
                                 std::unique_ptr<Atom> udta);
