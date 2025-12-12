//
//  mdhd_builder.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#pragma once
#include <ostream>

#include "mp4_atoms.hpp"

std::unique_ptr<Atom> build_mdhd(uint32_t timescale, uint64_t duration,
                                 uint16_t language = 0x55C4);  // default "und"
