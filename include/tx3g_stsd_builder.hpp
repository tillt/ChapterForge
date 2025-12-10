//
//  tx3g_stsd_builder.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//

#pragma once
#include "mp4_atoms.hpp"
#include <memory>

// Build a complete Apple-style tx3g SampleEntry for chapter titles.
std::unique_ptr<Atom> build_tx3g_sample_entry();
