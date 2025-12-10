//
//  parser.hpp
//  PlayEm
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#pragma once

#include <cstdint>
#include <istream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct Mp4AtomInfo {
  uint32_t type;
  uint64_t size;   // total atom size
  uint64_t offset; // offset in file
};

// Minimal parsed MP4 data for our authoring needs
struct ParsedMp4 {
  // Raw audio data from mdat
  std::vector<uint8_t> mdat_data;
  uint64_t mdat_size = 0;

  // ilst metadata atom payload (optional)
  std::vector<uint8_t> ilst_payload;

  // audio track timing info
  uint32_t audio_timescale = 0;
  uint64_t audio_duration = 0;

  // sample table (copied directly)
  std::vector<uint8_t> stsd;
  std::vector<uint8_t> stts;
  std::vector<uint8_t> stsc;
  std::vector<uint8_t> stsz;
  std::vector<uint8_t> stco;
};

// Utility: read big-endian 32-bit value
uint32_t read_u32(std::istream &in);

// Utility: read big-endian 64-bit value
uint64_t read_u64(std::istream &in);

// Main parsing entry point
ParsedMp4 parse_mp4(const std::string &path);
