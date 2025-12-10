//
//  smhd_builder.cpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//

#include "smhd_builder.hpp"

std::unique_ptr<Atom> build_smhd() {
  auto smhd = Atom::create("smhd");
  auto &p = smhd->payload;

  // version
  write_u8(p, 0);

  // flags
  write_u24(p, 0);

  // balance = 0
  write_u16(p, 0);

  // reserved
  write_u16(p, 0);

  return smhd;
}
