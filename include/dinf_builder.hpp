//
//  dinf_builder.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//

#pragma once
#include "mp4_atoms.hpp"
#include <memory>

// Build the standard Apple/ISO 'dinf' container:
//   dinf
//     dref
//       url  (flags = 1, self-contained)
std::unique_ptr<Atom> build_dinf();
