//
//  tx3g_stsd_builder.cpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//

#include "tx3g_stsd_builder.hpp"
#include <cstring>

std::unique_ptr<Atom> build_tx3g_sample_entry() {
  auto tx3g = Atom::create("tx3g");
  auto &p = tx3g->payload;

  // -----------------------------------------
  // SampleEntry Header (generic)
  // -----------------------------------------
  // 6 reserved bytes + data_reference_index
  write_u32(p, 0);
  write_u16(p, 0);
  write_u16(p, 1);

  // -----------------------------------------
  // TextSampleEntry body (ISO/Apple style)
  // -----------------------------------------

  // displayFlags (4)
  write_u32(p, 0x00000000); // autowrap, scroll disabled

  // Justification (2 bytes)
  write_u8(p, 0); // horizontal (0 = left)
  write_u8(p, 0); // vertical   (0 = top)

  // Background color RGBA (4 bytes)
  write_u8(p, 0x00);
  write_u8(p, 0x00);
  write_u8(p, 0x00);
  write_u8(p, 0x00); // transparent

  // Default text box (8 bytes)
  // top, left, bottom, right
  write_u16(p, 0);
  write_u16(p, 0);
  write_u16(p, 0);
  write_u16(p, 0);

  // -----------------------------------------
  // Default style record (12 bytes)
  // -----------------------------------------
  write_u16(p, 0x0000); // startChar
  write_u16(p, 0xFFFF); // endChar (entire sample)
  write_u16(p, 1);      // fontID
  write_u8(p, 0x00);    // font_face (plain)
  write_u8(p, 12);      // font_size
  write_u8(p, 0x00);    // text color RGBA
  write_u8(p, 0x00);
  write_u8(p, 0x00);
  write_u8(p, 0xFF); // opaque black

  // -----------------------------------------
  // FontTableBox ('ftab')
  // -----------------------------------------
  auto ftab = Atom::create("ftab");
  auto &f = ftab->payload;

  write_u16(f, 1); // entry_count
  write_u16(f, 1); // fontID = 1
  const char *fontName = "Helvetica";
  uint8_t len = static_cast<uint8_t>(strlen(fontName));
  write_u8(f, len); // name length
  f.insert(f.end(), fontName, fontName + len);
  f.push_back(0); // null terminator (matches common encoders)

  tx3g->add(std::move(ftab));

  return tx3g;
}
