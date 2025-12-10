//
//  stsz_builder.cpp
//  mp4chapters
//
//  Created by Till Toenshoff on 12/9/25.
//

#include "stsz_builder.hpp"

std::unique_ptr<Atom> build_stsz(const std::vector<uint32_t> &sample_sizes) {
  auto stsz = Atom::create("stsz");
  auto &p = stsz->payload;

  // version + flags
  write_u8(p, 0);
  write_u24(p, 0);

  // sample_size = 0  (means lookup table is used)
  write_u32(p, 0);

  // sample_count
  write_u32(p, (uint32_t)sample_sizes.size());

  // sizes
  for (uint32_t sz : sample_sizes)
    write_u32(p, sz);

  return stsz;
}
