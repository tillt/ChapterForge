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

#include "aac_extractor.hpp"
#include "meta_builder.hpp"
#include "mp4_muxer.hpp"
#include "stbl_image_builder.hpp"
#include "stbl_text_builder.hpp"

namespace chapterforge {

/// Mux AAC input + chapter/text/image metadata into an M4A file.
///
/// - Parameters:
///   - input_audio_path: Path to AAC (ADTS) or MP4/M4A containing AAC.
///   - chapter_json_path: Path to chapter JSON (titles, timestamps, optional images, optional urls).
///     If the input has an `ilst`, it is reused unless the JSON supplies metadata overrides.
///   - output_path: Destination .m4a file.
///   - fast_start: When true, places `moov` ahead of `mdat` (fast-start/streamable).
///
/// - Returns: true on success, false on failure.
bool mux_file_to_m4a(const std::string &input_audio_path,
					 const std::string &chapter_json_path,
                     const std::string &output_path,
					 bool fast_start = true);

/// Mux AAC input + in-memory chapter data (titles + images).
///
/// - Parameters:
///   - input_audio_path: Path to AAC (ADTS) or MP4/M4A containing AAC.
///   - text_chapters: Chapter title samples (`text` UTF-8, `start_ms` absolute).
///   - image_chapters: Optional JPEG data per chapter; leave empty to omit the image track.
///   - metadata: Top-level metadata; if empty and the input has `ilst`, it is reused.
///   - output_path: Destination .m4a file.
///   - fast_start: When true, places `moov` ahead of `mdat`.
///
/// - Returns: true on success, false on failure.
bool mux_file_to_m4a(const std::string &input_audio_path,
                     const std::vector<ChapterTextSample> &text_chapters,
                     const std::vector<ChapterImageSample> &image_chapters,
                     const MetadataSet &metadata, const std::string &output_path,
                     bool fast_start = true);

/// Same as above but omit images; no image track is created.
///
/// - Parameters:
///   - input_audio_path: Path to AAC (ADTS) or MP4/M4A containing AAC.
///   - text_chapters: Chapter title samples (`text` UTF-8, `start_ms` absolute).
///   - metadata: Top-level metadata; if empty and the input has `ilst`, it is reused.
///   - output_path: Destination .m4a file.
///   - fast_start: When true, places `moov` ahead of `mdat`.
///
/// - Returns: true on success, false on failure.
bool mux_file_to_m4a(const std::string &input_audio_path,
                     const std::vector<ChapterTextSample> &text_chapters,
                     const MetadataSet &metadata, const std::string &output_path,
                     bool fast_start = true);

/// Same as above but without providing metadata; reuses source `ilst` if present, otherwise
/// writes an empty `ilst`.
///
/// - Parameters:
///   - input_audio_path: Path to AAC (ADTS) or MP4/M4A containing AAC.
///   - text_chapters: Chapter title samples (`text` UTF-8, `start_ms` absolute).
///   - image_chapters: Optional JPEG data per chapter; leave empty to omit the image track.
///   - output_path: Destination .m4a file.
///   - fast_start: When true, places `moov` ahead of `mdat`.
///
/// - Returns: true on success, false on failure.
bool mux_file_to_m4a(const std::string &input_audio_path,
                     const std::vector<ChapterTextSample> &text_chapters,
                     const std::vector<ChapterImageSample> &image_chapters,
                     const std::string &output_path, bool fast_start = true);

/// Mux AAC input + in-memory chapter data (with optional URL track).
///
/// - Parameters:
///   - input_audio_path: Path to AAC (ADTS) or MP4/M4A containing AAC.
///   - text_chapters: Chapter titles (`text`/`start_ms`; `href` unused here).
///   - url_chapters: Optional URL track; set `href` per sample, `text` may be empty. Leave empty to
///     skip the URL track.
///   - image_chapters: Optional JPEG data per chapter; leave empty to omit the image track.
///   - output_path: Destination .m4a file.
///   - fast_start: When true, places `moov` ahead of `mdat`.
///
/// - Returns: true on success, false on failure.
bool mux_file_to_m4a(const std::string &input_audio_path,
                     const std::vector<ChapterTextSample> &text_chapters,
                     const std::vector<ChapterTextSample> &url_chapters,
                     const std::vector<ChapterImageSample> &image_chapters,
					 const std::string &output_path,
                     bool fast_start = true);

/// Mux AAC input + in-memory chapter data (with optional URL track).
///
/// - Parameters:
///   - input_audio_path: Path to AAC (ADTS) or MP4/M4A containing AAC.
///   - text_chapters: Chapter titles (`text`/`start_ms`; `href` unused here).
///   - url_chapters: Optional URL track; set `href` per sample, `text` may be empty. Leave empty to
///     skip the URL track.
///   - image_chapters: Optional JPEG data per chapter; leave empty to omit the image track.
///   - metadata: Top-level metadata; reused from input `ilst` if empty.
///   - output_path: Destination .m4a file.
///   - fast_start: When true, places `moov` ahead of `mdat`.
///
/// - Returns: true on success, false on failure.
bool mux_file_to_m4a(const std::string &input_audio_path,
                     const std::vector<ChapterTextSample> &text_chapters,
                     const std::vector<ChapterTextSample> &url_chapters,
                     const std::vector<ChapterImageSample> &image_chapters,
                     const MetadataSet &metadata, const std::string &output_path,
                     bool fast_start = true);

}  // namespace chapterforge
