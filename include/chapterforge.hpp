//
//  chapterforge.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#pragma once
#include <cstdint>
#include <stdint.h>
#include <string>
#include <vector>

#include "chapter_image_sample.hpp"
#include "chapter_text_sample.hpp"
#include "metadata_set.hpp"

namespace chapterforge {

/// @defgroup api ChapterForge Public API
/// Public, supported C++ interfaces for muxing chapters into M4A files.
/// @{

/**
 * @brief Mux AAC input + chapter/text/image metadata into an M4A file.
 *
 * @param input_audio_path Path to AAC (ADTS) or MP4/M4A containing AAC.
 * @param chapter_json_path Path to chapter JSON (titles, timestamps, optional images, optional urls).
 *        If the input has an ilst, it is reused unless the JSON supplies metadata overrides.
 * @param output_path Destination .m4a file.
 * @param fast_start When true, places moov ahead of mdat (fast-start/streamable).
 * @return true on success, false on failure.
 */
bool mux_file_to_m4a(const std::string &input_audio_path,
					 const std::string &chapter_json_path,
                     const std::string &output_path,
					 bool fast_start = true);  ///< @ingroup api

/**
 * @brief Mux AAC input + in-memory chapter data (titles + images).
 *
 * @param input_audio_path Path to AAC (ADTS) or MP4/M4A containing AAC.
 * @param text_chapters Chapter title samples (text UTF-8, start_ms absolute).
 * @param image_chapters Optional JPEG data per chapter; leave empty to omit the image track.
 * @param metadata Top-level metadata; if empty and the input has ilst, it is reused.
 * @param output_path Destination .m4a file.
 * @param fast_start When true, places moov ahead of mdat.
 * @return true on success, false on failure.
 */
bool mux_file_to_m4a(const std::string &input_audio_path,
                     const std::vector<ChapterTextSample> &text_chapters,
                     const std::vector<ChapterImageSample> &image_chapters,
                     const MetadataSet &metadata, const std::string &output_path,
                     bool fast_start = true);  ///< @ingroup api

/// @overload
bool mux_file_to_m4a(const std::string &input_audio_path,
                     const std::vector<ChapterTextSample> &text_chapters,
                     const MetadataSet &metadata, const std::string &output_path,
                     bool fast_start = true);  ///< @ingroup api

/**
 * @brief Same as the primary overload but without providing metadata (reuses source ilst if any).
 *
 * @param input_audio_path Path to AAC (ADTS) or MP4/M4A containing AAC.
 * @param text_chapters Chapter title samples (text UTF-8, start_ms absolute).
 * @param image_chapters Optional JPEG data per chapter; leave empty to omit the image track.
 * @param output_path Destination .m4a file.
 * @param fast_start When true, places moov ahead of mdat.
 * @return true on success, false on failure.
 */
bool mux_file_to_m4a(const std::string &input_audio_path,
                     const std::vector<ChapterTextSample> &text_chapters,
                     const std::vector<ChapterImageSample> &image_chapters,
                     const std::string &output_path, bool fast_start = true);  ///< @ingroup api

/**
 * @brief Mux AAC input + in-memory chapter data (with optional URL track).
 *
 * @param input_audio_path Path to AAC (ADTS) or MP4/M4A containing AAC.
 * @param text_chapters Chapter titles (text/start_ms; href unused here).
 * @param url_chapters Optional URL track; set href per sample; text (or url_text in JSON) is
 *        optional and defaults to empty to match Apple-authored files. Leave empty to skip the URL track.
 * @param image_chapters Optional JPEG data per chapter; leave empty to omit the image track.
 * @param output_path Destination .m4a file.
 * @param fast_start When true, places moov ahead of mdat.
 * @return true on success, false on failure.
 */
bool mux_file_to_m4a(const std::string &input_audio_path,
                     const std::vector<ChapterTextSample> &text_chapters,
                     const std::vector<ChapterTextSample> &url_chapters,
                     const std::vector<ChapterImageSample> &image_chapters,
					 const std::string &output_path,
                     bool fast_start = true);  ///< @ingroup api

/**
 * @brief Variant with explicit metadata; otherwise identical to the overload above.
 *
 * @param input_audio_path Path to AAC (ADTS) or MP4/M4A containing AAC.
 * @param text_chapters Chapter titles (text/start_ms; href optional).
 * @param url_chapters Optional URL track (href/text per sample); leave empty to omit.
 * @param image_chapters Optional JPEG data per chapter; leave empty to omit.
 * @param metadata Top-level metadata; reused from input ilst if empty.
 * @param output_path Destination .m4a file.
 * @param fast_start When true, places moov ahead of mdat.
 * @return true on success, false on failure.
 */
bool mux_file_to_m4a(const std::string &input_audio_path,
                     const std::vector<ChapterTextSample> &text_chapters,
                     const std::vector<ChapterTextSample> &url_chapters,
                     const std::vector<ChapterImageSample> &image_chapters,
                     const MetadataSet &metadata, const std::string &output_path,
                     bool fast_start = true);  ///< @ingroup api

/// @}

}  // namespace chapterforge
