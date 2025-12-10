//
//  udta_builder.hpp.
//  ChapterForge.
//
//  Created by Till Toenshoff on 12/9/25.
//

#pragma once
#include <memory>

#include "mp4_atoms.hpp"
#include "stbl_text_builder.hpp"

std::unique_ptr<Atom> build_udta(std::unique_ptr<Atom> meta,
                                 const std::vector<ChapterTextSample> &chapters);
