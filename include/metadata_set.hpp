//
//  metadata_set.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#pragma once

#include <string>
#include <vector>

/**
 * @brief Top-level metadata container for ilst.
 *
 * Fields are UTF-8; cover holds JPEG data.
 */
struct MetadataSet {
    std::string title;          ///< Track title
    std::string artist;         ///< Artist/author
    std::string album;          ///< Album/collection
    std::string genre;          ///< Genre tag
    std::string year;           ///< Year (free-form)
    std::string comment;        ///< Comment/description
    std::vector<uint8_t> cover; ///< JPEG cover data
};
