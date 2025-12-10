//
//  mdhd_builder.hpp
//  mp4chapters
//
//  Created by Till Toenshoff on 12/9/25.
//

#pragma once
#include "mp4_atoms.hpp"
#include <ostream>

std::unique_ptr<Atom> build_mdhd(uint32_t timescale, uint64_t duration,
                                 uint16_t language = 0x55C4); // default "und"
