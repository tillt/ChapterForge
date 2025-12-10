//
//  stsd_builder.cpp.
//  ChapterForge.
//
//  Created by Till Toenshoff on 12/9/25.
//

#include "stsd_builder.hpp"

#include "mp4_atoms.hpp"

std::unique_ptr<Atom> build_stsd_mp4a(const Mp4aConfig &cfg) {
    auto stsd = Atom::create("stsd");
    auto &p = stsd->payload;

    // version + flags.
    write_u8(p, 0);
    write_u24(p, 0);

    // entry_count = 1.
    write_u32(p, 1);

    // mp4a sample entry.
    stsd->add(build_mp4a(cfg));

    return stsd;
}
