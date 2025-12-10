//
//  stsd_builder.hpp.
//  ChapterForge.
//
//  Created by Till Toenshoff on 12/9/25.
//

#pragma once
#include <memory>

#include "mp4_atoms.hpp"
#include "mp4a_builder.hpp"

std::unique_ptr<Atom> build_stsd_mp4a(const Mp4aConfig &cfg);
