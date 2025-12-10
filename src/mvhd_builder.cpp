//
//  mvhd_builder.cpp.
//  ChapterForge.
//
//  Created by Till Toenshoff on 12/9/25.
//

#include "mvhd_builder.hpp"

std::unique_ptr<Atom> build_mvhd(uint32_t timescale, uint64_t duration) {
    auto mvhd = Atom::create("mvhd");

    // Size: Apple uses 100 bytes for mvhd version 0.
    std::vector<uint8_t> &p = mvhd->payload;
    p.reserve(100);

    // version (0) + flags (0)
    write_u8(p, 0);
    write_u24(p, 0);

    // creation_time.
    write_u32(p, 0);
    write_u32(p, 0);

    // timescale + duration.
    write_u32(p, timescale);
    write_u32(p, (uint32_t)duration);

    // rate = 1.0 (0x00010000)
    write_u32(p, 0x00010000);

    // volume = 1.0 (0x0100)
    write_u16(p, 0x0100);

    // reserved.
    write_u16(p, 0);
    write_u64(p, 0);

    // matrix (identity)
    static const uint8_t matrix[36] = {0x00, 0x01, 0x00, 0x00, 0,    0, 0, 0, 0, 0, 0,
                                       0,    0x00, 0x01, 0x00, 0x00, 0, 0, 0, 0, 0, 0,
                                       0,    0,    0,    0,    0,    0, 0, 0, 0, 0};
    p.insert(p.end(), matrix, matrix + 36);

    // pre-defined (24)
    for (int i = 0; i < 24; i++) {
        p.push_back(0);
    }

    // next_track_ID.
    write_u32(p, 5);

    return mvhd;
}
