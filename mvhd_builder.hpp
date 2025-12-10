//
//  mvhd_builder.hpp
//  mp4chapters
//
//  Created by Till Toenshoff on 12/9/25.
//

#pragma once
#include "mp4_atoms.hpp"
#include <memory>

std::unique_ptr<Atom> build_mvhd(uint32_t timescale, uint64_t duration);
