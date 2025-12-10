//
//  minf_builder.hpp.
//  ChapterForge.
//
//  Created by Till Toenshoff on 12/9/25.
//

#pragma once
#include <memory>

#include "mp4_atoms.hpp"

// Build Media Information Box for:
//  - audio (smhd)
//  - text chapters (nmhd)
//  - image chapters (vmhd)
std::unique_ptr<Atom> build_minf_audio(std::unique_ptr<Atom> stbl);
std::unique_ptr<Atom> build_minf_text(std::unique_ptr<Atom> stbl);
std::unique_ptr<Atom> build_minf_image(std::unique_ptr<Atom> stbl);
