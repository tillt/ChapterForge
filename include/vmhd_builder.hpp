//
//  vmhd_builder.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//

#pragma once
#include "mp4_atoms.hpp"
#include <memory>

// Build a Video Media Header Box ('vmhd')
// Required for image (jpeg) chapter track.
std::unique_ptr<Atom> build_vmhd();
