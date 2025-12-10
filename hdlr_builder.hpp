//
//  hdlr_builder.hpp
//  mp4chapters
//
//  Created by Till Toenshoff on 12/9/25.
//

#pragma once
#include "mp4_atoms.hpp"
#include <memory>

// Apple-style handler boxes:
//
// Audio track:
//   handler_type = "soun"
//   handler_name = "SoundHandler"
//
// Text chapter track:
//   handler_type = "text"
//   handler_name = "Chapter Titles"
//
// Image chapter track:
//   handler_type = "vide"
//   handler_name = "Chapter Images"

std::unique_ptr<Atom> build_hdlr_sound();
std::unique_ptr<Atom> build_hdlr_text();
std::unique_ptr<Atom> build_hdlr_video();
