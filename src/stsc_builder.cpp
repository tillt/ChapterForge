//
//  stsc_builder.cpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//

#include "stsc_builder.hpp"

std::unique_ptr<Atom> build_stsc(uint32_t sample_count) {
  auto stsc = Atom::create("stsc");
  auto &p = stsc->payload;

  // version + flags
  write_u8(p, 0);
  write_u24(p, 0);

  // entry_count = 1
  write_u32(p, 1);

  // first_chunk = 1
  write_u32(p, 1);

  // samples_per_chunk = sample_count
  write_u32(p, sample_count);

  // sample_description_index = 1
  write_u32(p, 1);

  return stsc;
}
