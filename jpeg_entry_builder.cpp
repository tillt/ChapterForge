//
//  jpeg_entry_builder.cpp
//  mp4chapters
//
//  Created by Till Toenshoff on 12/9/25.
//

#include "jpeg_entry_builder.hpp"
#include "mp4_atoms.hpp"

std::unique_ptr<Atom> build_stsd_jpeg(uint16_t width, uint16_t height) {
  auto stsd = Atom::create("stsd");
  auto &p = stsd->payload;

  write_u8(p, 0);
  write_u24(p, 0);

  write_u32(p, 1); // one sample entry

  // jpeg sample entry
  auto jpeg = Atom::create("jpeg");
  auto &q = jpeg->payload;

  // SampleEntry header
  write_u32(q, 0); // reserved 6 bytes (4 + 2)
  write_u16(q, 0);
  write_u16(q, 1); // data_reference_index

  // QuickTime-compatible JPEG still image sample entry:

  write_u16(q, 0); // version
  write_u16(q, 0); // revision
  write_u32(q, 0); // vendor

  write_u32(q, 0); // temporal_quality
  write_u32(q, 0); // spatial_quality

  // Width/height for still image display
  write_u16(q, width);
  write_u16(q, height);

  write_u32(q, 0x00480000); // horizresolution (72dpi)
  write_u32(q, 0x00480000); // vertresolution (72dpi)

  write_u32(q, 0); // data size
  write_u16(q, 1); // frame count

  // compressorname (pascal string)
  const char *name = "JPEG";
  uint8_t len = 4;
  write_u8(q, len);
  q.insert(q.end(), name, name + len);
  for (int i = 0; i < (31 - len); i++)
    q.push_back(0);

  write_u16(q, 24); // depth
  write_u16(q, -1); // color table = no color table

  stsd->add(std::move(jpeg));
  return stsd;
}

// Build only the JPEG sample entry (used by older stbl builder)
std::unique_ptr<Atom> build_jpeg_sample_entry(uint16_t width, uint16_t height) {
  auto jpeg = Atom::create("jpeg");
  auto &q = jpeg->payload;

  // SampleEntry header
  write_u32(q, 0); // 4 reserved bytes
  write_u16(q, 0); // 2 reserved bytes
  write_u16(q, 1); // data_reference_index

  write_u16(q, 0); // version
  write_u16(q, 0); // revision
  write_u32(q, 0); // vendor

  write_u32(q, 0); // temporal_quality
  write_u32(q, 0); // spatial_quality

  write_u16(q, width);
  write_u16(q, height);

  write_u32(q, 0x00480000); // horizresolution (72dpi)
  write_u32(q, 0x00480000); // vertresolution (72dpi)

  write_u32(q, 0); // data size
  write_u16(q, 1); // frame count

  const char *name = "JPEG";
  uint8_t len = 4;
  write_u8(q, len);
  q.insert(q.end(), name, name + len);
  for (int i = 0; i < (31 - len); i++)
    q.push_back(0);

  write_u16(q, 24);                        // depth
  write_u16(q, static_cast<uint16_t>(-1)); // color table = no color table

  return jpeg;
}
