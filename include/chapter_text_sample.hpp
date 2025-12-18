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

/**
 * @brief Represents a chapter title/URL sample.
 *
 * @param text UTF-8 chapter title (or URL text, if used on a URL track).
 * @param href Optional hyperlink URL (tx3g modifier). Leave empty for plain title.
 * @param start_ms Absolute start time in milliseconds.
 */
struct ChapterTextSample {
    std::string text;       ///< UTF-8 text
    std::string href;       ///< Optional hyperlink URL (tx3g modifier)
    uint32_t start_ms = 0;  ///< Absolute start time in ms
};
