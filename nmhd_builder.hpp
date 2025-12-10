//
//  nmhd_builder.hpp
//  mp4chapters
//
//  Created by Till Toenshoff on 12/9/25.
//

#pragma once
#include "mp4_atoms.hpp"
#include <memory>

// Build Null Media Header Box ('nmhd') for the text chapter track.
std::unique_ptr<Atom> build_nmhd();
