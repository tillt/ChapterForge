//
//  smhd_builder.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#pragma once
#include <memory>

#include "mp4_atoms.hpp"

// Build Sound Media Header Box ('smhd') for the audio track.
std::unique_ptr<Atom> build_smhd();
