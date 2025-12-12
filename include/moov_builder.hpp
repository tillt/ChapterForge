//
//  moov_builder.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#pragma once
#include <memory>
#include <vector>

#include "mp4_atoms.hpp"

std::unique_ptr<Atom> build_moov(
    uint32_t timescale, uint64_t duration_ts, std::unique_ptr<Atom> trak_audio,
    std::vector<std::unique_ptr<Atom>> text_tracks, std::unique_ptr<Atom> trak_image,
    std::unique_ptr<Atom> udta);
