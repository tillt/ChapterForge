//
//  vmhd_builder.cpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//

#include "vmhd_builder.hpp"

std::unique_ptr<Atom> build_vmhd() {
  auto vmhd = Atom::create("vmhd");
  auto &p = vmhd->payload;

  // version = 0
  write_u8(p, 0);

  // flags = 1 (VERY IMPORTANT! Apple requires this)
  write_u24(p, 1);

  // graphicsmode = 0
  write_u16(p, 0);

  // opcolor (3 × 16-bit)
  write_u16(p, 0); // red
  write_u16(p, 0); // green
  write_u16(p, 0); // blue

  return vmhd;
}
