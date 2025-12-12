//
//  dinf_builder.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#pragma once
#include <memory>

#include "mp4_atoms.hpp"

// Build the standard Apple/ISO 'dinf' container:
//   dinf.
//     dref.
//       url  (flags = 1, self-contained)
std::unique_ptr<Atom> build_dinf();
