//
//  dinf_builder.cpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//

#include "dinf_builder.hpp"

// Builds:
// dinf
//   dref
//     url  (flags = 1, self-contained)
std::unique_ptr<Atom> build_dinf() {
  // --- url ---
  auto url = Atom::create("url ");
  auto &u = url->payload;

  write_u8(u, 0);  // version
  write_u24(u, 1); // flags = 1 (self-contained)

  // No payload for url when flags = 1

  // --- dref ---
  auto dref = Atom::create("dref");
  auto &d = dref->payload;

  write_u8(d, 0);  // version
  write_u24(d, 0); // flags
  write_u32(d, 1); // entry_count = 1

  dref->add(std::move(url));

  // --- dinf ---
  auto dinf = Atom::create("dinf");
  dinf->add(std::move(dref));

  return dinf;
}
