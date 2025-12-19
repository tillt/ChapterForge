//
//  chapter_text_sample.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#pragma once

#include <cstdint>
#include <string>

/// @ingroup api
/// Represents a chapter title/URL sample.
struct ChapterTextSample {
    std::string text;       ///< UTF-8 text
    std::string href;       ///< Optional hyperlink URL (tx3g modifier)
    uint32_t start_ms = 0;  ///< Absolute start time in ms
};
