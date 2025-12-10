// Minimal public API for embedding ChapterForge as a library.
// Provides a convenience end-to-end mux helper that takes file paths.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "aac_extractor.hpp"
#include "meta_builder.hpp"
#include "mp4_muxer.hpp"
#include "stbl_image_builder.hpp"
#include "stbl_text_builder.hpp"

namespace mp4chapters {

// Mux AAC input + chapter/text/image metadata into an M4A file.
//
// Parameters:
//   input_audio_path   - Path to AAC (ADTS) or MP4/M4A containing AAC.
//   chapter_json_path  - Path to chapter JSON (text, timestamps, optional images).
//   output_path        - Destination .m4a file.
//
// Returns true on success, false on failure.
bool mux_file_to_m4a(const std::string &input_audio_path,
                     const std::string &chapter_json_path,
                     const std::string &output_path);

// Mux AAC input + in-memory chapter data (no JSON parsing).
bool mux_file_to_m4a(const std::string &input_audio_path,
                     const std::vector<ChapterTextSample> &text_chapters,
                     const std::vector<ChapterImageSample> &image_chapters,
                     const MetadataSet &metadata,
                     const std::string &output_path);

} // namespace mp4chapters
