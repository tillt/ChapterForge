//
//  stsc_builder.hpp
//  mp4chapters
//
//  Created by Till Toenshoff on 12/9/25.
//

#pragma once
#include "mp4_atoms.hpp"
#include <memory>

std::unique_ptr<Atom> build_stsc(uint32_t sample_count);
