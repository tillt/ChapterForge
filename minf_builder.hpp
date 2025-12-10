//
//  minf_builder.hpp
//  mp4chapters
//
//  Created by Till Toenshoff on 12/9/25.
//

#pragma once
#include "mp4_atoms.hpp"
#include <memory>

// Build Media Information Box for:
//  - audio (smhd)
//  - text chapters (nmhd)
//  - image chapters (vmhd)
std::unique_ptr<Atom> build_minf_audio(std::unique_ptr<Atom> stbl);
std::unique_ptr<Atom> build_minf_text(std::unique_ptr<Atom> stbl);
std::unique_ptr<Atom> build_minf_image(std::unique_ptr<Atom> stbl);
