//
//  stts_builder.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//

#pragma once
#include "mp4_atoms.hpp"
#include <memory>
#include <vector>

std::unique_ptr<Atom> build_stts(const std::vector<uint32_t> &timestamps,
                                 uint32_t total_duration_ts);
