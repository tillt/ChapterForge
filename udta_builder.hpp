//
//  udta_builder.hpp
//  mp4chapters
//
//  Created by Till Toenshoff on 12/9/25.
//

#pragma once
#include "mp4_atoms.hpp"
#include "stbl_text_builder.hpp"
#include <memory>

std::unique_ptr<Atom>
build_udta(std::unique_ptr<Atom> meta,
           const std::vector<ChapterTextSample> &chapters);
