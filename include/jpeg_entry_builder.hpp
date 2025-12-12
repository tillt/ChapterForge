//
//  jpeg_entry_builder.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#pragma once
#include <memory>

#include "mp4_atoms.hpp"

// Build an stsd with a single JPEG sample entry, using the provided display.
// width/height for the sample entry fields.
std::unique_ptr<Atom> build_stsd_jpeg(uint16_t width, uint16_t height);
std::unique_ptr<Atom> build_jpeg_sample_entry(uint16_t width, uint16_t height);
