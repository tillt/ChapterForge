//
//  hdlr_builder.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#pragma once
#include <memory>
#include <string>

#include "mp4_atoms.hpp"

// Apple-style handler boxes:
//
// Audio track:
//   handler_type = "soun"
//   handler_name = "SoundHandler"
//
// Text chapter tracks:
//   handler_type = "text"
//   handler_name = e.g. "Chapter Titles", "Chapter Artist"
//
// Image chapter track:
//   handler_type = "vide"
//   handler_name = "Chapter Images"

std::unique_ptr<Atom> build_hdlr_sound();
std::unique_ptr<Atom> build_hdlr_text(const std::string &name = "Chapter Titles");
std::unique_ptr<Atom> build_hdlr_metadata(const std::string &name = "Chapter Metadata");
std::unique_ptr<Atom> build_hdlr_video();
