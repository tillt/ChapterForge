//
//  stsz_builder.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#pragma once
#include <memory>
#include <vector>

#include "mp4_atoms.hpp"

std::unique_ptr<Atom> build_stsz(const std::vector<uint32_t> &sample_sizes);
