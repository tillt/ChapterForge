//
//  tkhd_builder.cpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//

#include "tkhd_builder.hpp"

static std::unique_ptr<Atom>
build_tkhd_common(uint32_t track_id, uint64_t duration, uint8_t flags3,
                  uint16_t volume, uint16_t layer, float width, float height) {
  auto tkhd = Atom::create("tkhd");
  std::vector<uint8_t> &p = tkhd->payload;

  // Apple uses 84 bytes
  p.reserve(84);

  write_u8(p, 0);       // version
  write_u24(p, flags3); // flags

  write_u32(p, 0); // creation_time
  write_u32(p, 0); // modification_time

  write_u32(p, track_id); // track_ID
  write_u32(p, 0);        // reserved

  write_u32(p, (uint32_t)duration);

  write_u64(p, 0); // reserved[8]

  write_u16(p, layer); // layer
  write_u16(p, 0);     // alternate_group

  write_u16(p, volume); // 0x0100 for audio, 0 for others
  write_u16(p, 0);      // reserved

  // unity matrix (same as QuickTime defaults)
  write_u32(p, 0x00010000); // a
  write_u32(p, 0x00000000); // b
  write_u32(p, 0x00000000); // u
  write_u32(p, 0x00000000); // c
  write_u32(p, 0x00010000); // d
  write_u32(p, 0x00000000); // v
  write_u32(p, 0x00000000); // x
  write_u32(p, 0x00000000); // y
  write_u32(p, 0x40000000); // w (scale)

  // width, height (16.16 fixed)
  write_fixed16_16(p, width);
  write_fixed16_16(p, height);

  return tkhd;
}

std::unique_ptr<Atom> build_tkhd_audio(uint32_t track_id, uint64_t duration) {
  return build_tkhd_common(track_id, duration, 7, 0x0100, 0, 0.0f, 0.0f);
}

std::unique_ptr<Atom> build_tkhd_text(uint32_t track_id, uint64_t duration) {
  return build_tkhd_common(track_id, duration, 1, 0x0000, 0, 0.0f, 0.0f);
}

std::unique_ptr<Atom> build_tkhd_image(uint32_t track_id, uint64_t duration,
                                       uint16_t width, uint16_t height) {
  // Mark as enabled/in-movie/in-preview (flags=7) so players like QuickTime
  // consider it for display as the visual track.
  return build_tkhd_common(track_id, duration, 7, 0x0000, 0, (float)width,
                           (float)height);
}
