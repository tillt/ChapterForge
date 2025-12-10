//
//  nmhd_builder.hpp.
//  ChapterForge.
//
//  Created by Till Toenshoff on 12/9/25.
//

#pragma once
#include <memory>

#include "mp4_atoms.hpp"

// Build Null Media Header Box ('nmhd') for the text chapter track.
std::unique_ptr<Atom> build_nmhd();
