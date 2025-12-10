//
//  mdhd_builder.cpp.
//  ChapterForge.
//
//  Created by Till Toenshoff on 12/9/25.
//

#include "mdhd_builder.hpp"

std::unique_ptr<Atom> build_mdhd(uint32_t timescale, uint64_t duration, uint16_t language) {
    auto mdhd = Atom::create("mdhd");
    std::vector<uint8_t> &p = mdhd->payload;

    p.reserve(32);

    write_u8(p, 0);   // version
    write_u24(p, 0);  // flags

    write_u32(p, 0);  // creation_time
    write_u32(p, 0);  // modification_time

    write_u32(p, timescale);
    write_u32(p, (uint32_t)duration);

    // language.
    write_u16(p, language);

    write_u16(p, 0);  // pre-defined

    return mdhd;
}
