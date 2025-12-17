//
//  mp4_muxer.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <utility>

#include "aac_extractor.hpp"
#include "metadata_set.hpp"
#include "mp4_atoms.hpp"
#include "mp4a_builder.hpp"
#include "stbl_image_builder.hpp"
#include "stbl_text_builder.hpp"

// Complete MP4 writer: takes raw AAC (ADTS) bytes, chapter text/image samples,
// audio config, and metadata.
// Exposed for embedding; prefer the higher-level helper in chapterforge.hpp.
// when linking externally.
bool write_mp4(const std::string &path, const AacExtractResult &aac,
               const std::vector<ChapterTextSample> &text_chapters,
               const std::vector<ChapterImageSample> &image_chapters, Mp4aConfig audio_cfg,
               const MetadataSet &meta, bool fast_start = true,
               const std::vector<std::pair<std::string, std::vector<ChapterTextSample>>>
                   &extra_text_tracks = {},
               const std::vector<uint8_t> *ilst_payload = nullptr);
