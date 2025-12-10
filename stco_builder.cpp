//
//  stco_builder.cpp
//  mp4chapters
//
//  Created by Till Toenshoff on 12/9/25.
//

#include "stco_builder.hpp"

std::unique_ptr<Atom> build_stco() {
  auto stco = Atom::create("stco");
  auto &p = stco->payload;

  write_u8(p, 0);  // version
  write_u24(p, 0); // flags

  write_u32(p, 1); // entry_count = 1

  // offset placeholder (to be patched by muxer)
  write_u32(p, 0);

  return stco;
}
