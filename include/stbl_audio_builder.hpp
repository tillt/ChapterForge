//
//  stbl_audio_builder.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//

#pragma once
#include "mp4_atoms.hpp"
#include "mp4a_builder.hpp"
#include <memory>
#include <vector>

std::unique_ptr<Atom>
build_audio_stbl(const Mp4aConfig &cfg,
                 const std::vector<uint32_t> &sample_sizes,
                 const std::vector<uint32_t> &chunk_sizes, uint32_t num_samples,
                 const std::vector<uint8_t> *raw_stsd = nullptr);

// Build stbl from pre-existing box payloads (stsd/stts/stsc/stsz/stco)
std::unique_ptr<Atom>
build_audio_stbl_raw(const std::vector<uint8_t> &stsd_payload,
                     const std::vector<uint8_t> &stts_payload,
                     const std::vector<uint8_t> &stsc_payload,
                     const std::vector<uint8_t> &stsz_payload,
                     const std::vector<uint8_t> &stco_payload);
