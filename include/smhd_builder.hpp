//
//  smhd_builder.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//

#pragma once
#include "mp4_atoms.hpp"
#include <memory>

// Build Sound Media Header Box ('smhd') for the audio track.
std::unique_ptr<Atom> build_smhd();
