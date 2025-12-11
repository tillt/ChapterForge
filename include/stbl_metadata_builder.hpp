//
//  stbl_metadata_builder.hpp.
//  ChapterForge.
//
//  Builds a timed-metadata (mdta) sample table using a simple text-based 'mett'
//  sample entry. Each sample payload is arbitrary bytes (we emit UTF-8 text).
//

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "mp4_atoms.hpp"

// Represents a timed metadata sample aligned to chapter timing.
struct ChapterMetadataSample {
    std::string payload;   // UTF-8 content
    uint32_t start_ms = 0; // absolute start time in ms
};

std::unique_ptr<Atom> build_metadata_stbl(const std::vector<ChapterMetadataSample> &samples,
                                          uint32_t track_timescale);
