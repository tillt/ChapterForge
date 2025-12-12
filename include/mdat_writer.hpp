//
//  mdat_writer.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#pragma once
#include <cstdint>
#include <fstream>
#include <vector>

#include "mp4_atoms.hpp"

// Stores final chunk offsets per track, used for STCO patching.
struct MdatOffsets {
    std::vector<uint32_t> audio_offsets;
    std::vector<std::vector<uint32_t>> text_offsets;  // one entry per text track
    std::vector<uint32_t> image_offsets;

    uint64_t payload_start = 0;  // absolute file offset where mdat payload begins
};

// Write mdat and return offsets (relative to payload_start)
MdatOffsets write_mdat(std::ofstream &out, const std::vector<std::vector<uint8_t>> &audio_samples,
                       const std::vector<std::vector<std::vector<uint8_t>>> &text_tracks_samples,
                       const std::vector<std::vector<uint8_t>> &image_samples,
                       const std::vector<uint32_t> &audio_chunk_sizes,
                       const std::vector<std::vector<uint32_t>> &text_chunk_sizes,
                       const std::vector<uint32_t> &image_chunk_sizes);

// Patch a single stco atom.
void patch_stco_table(Atom *stco, const std::vector<uint32_t> &offsets,
                      uint64_t mdat_payload_start);

// Patch stco boxes in moov (audio, text tracks, image). If patch_audio is false,
// audio stco (first one) is left untouched.
void patch_all_stco(Atom *moov, const MdatOffsets &offs, bool patch_audio = true);

// Compute chunk offsets without writing, given starting payload offset.
MdatOffsets compute_mdat_offsets(uint64_t payload_start,
                                 const std::vector<std::vector<uint8_t>> &audio_samples,
                                 const std::vector<std::vector<std::vector<uint8_t>>> &text_tracks_samples,
                                 const std::vector<std::vector<uint8_t>> &image_samples,
                                 const std::vector<uint32_t> &audio_chunk_sizes,
                                 const std::vector<std::vector<uint32_t>> &text_chunk_sizes,
                                 const std::vector<uint32_t> &image_chunk_sizes);
