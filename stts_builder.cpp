//
//  stts_builder.cpp
//  mp4chapters
//
//  Created by Till Toenshoff on 12/9/25.
//

#include "stts_builder.hpp"

std::unique_ptr<Atom> build_stts(const std::vector<uint32_t> &timestamps,
                                 uint32_t total_duration_ts) {
  auto stts = Atom::create("stts");
  auto &p = stts->payload;

  size_t N = timestamps.size();
  if (N == 0)
    throw std::runtime_error("stts: need at least 1 timestamp");

  // version + flags
  write_u8(p, 0);
  write_u24(p, 0);

  // entry_count = N
  write_u32(p, (unsigned int)N);

  for (size_t i = 0; i < N; i++) {
    uint32_t dur = 0;

    if (i + 1 < N)
      dur = timestamps[i + 1] - timestamps[i];
    else
      dur = total_duration_ts - timestamps[i];

    write_u32(p, 1);   // sample_count (one sample per chapter)
    write_u32(p, dur); // sample_delta
  }

  return stts;
}
