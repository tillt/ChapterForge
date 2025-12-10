//
//  stsz_builder.hpp
//  mp4chapters
//
//  Created by Till Toenshoff on 12/9/25.
//

#pragma once
#include "mp4_atoms.hpp"
#include <memory>
#include <vector>

std::unique_ptr<Atom> build_stsz(const std::vector<uint32_t> &sample_sizes);
