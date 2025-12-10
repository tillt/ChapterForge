//
//  udta_builder.cpp
//  mp4chapters
//
//  Created by Till Toenshoff on 12/9/25.
//

#include "udta_builder.hpp"

static std::unique_ptr<Atom>
build_chpl(const std::vector<ChapterTextSample> &chapters) {
  auto chpl = Atom::create("chpl");
  auto &p = chpl->payload;

  write_u8(p, 0);  // version
  write_u24(p, 0); // flags

  uint8_t count = static_cast<uint8_t>(std::min<size_t>(chapters.size(), 255));
  write_u8(p, count);

  uint64_t start = 0;
  for (size_t i = 0; i < count; ++i) {
    // timestamp in milliseconds (64-bit)
    uint64_t ts = start;
    write_u64(p, ts);

    const std::string &title = chapters[i].text;
    uint8_t len = static_cast<uint8_t>(std::min<size_t>(title.size(), 255));
    write_u8(p, len);
    p.insert(p.end(), title.begin(), title.begin() + len);

    start += chapters[i].duration_ms;
  }

  return chpl;
}

std::unique_ptr<Atom>
build_udta(std::unique_ptr<Atom> meta,
           const std::vector<ChapterTextSample> &chapters) {
  auto udta = Atom::create("udta");
  udta->add(std::move(meta));
  udta->add(build_chpl(chapters));
  return udta;
}
