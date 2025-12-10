//
//  nmhd_builder.cpp.
//  ChapterForge.
//
//  Created by Till Toenshoff on 12/9/25.
//

#include "nmhd_builder.hpp"

std::unique_ptr<Atom> build_nmhd() {
    auto nmhd = Atom::create("nmhd");
    auto &p = nmhd->payload;

    // version.
    write_u8(p, 0);

    // flags.
    write_u24(p, 0);

    // no more fields for nmhd.
    return nmhd;
}
