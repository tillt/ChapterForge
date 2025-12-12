//
//  tx3g_stsd_builder.cpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#include "tx3g_stsd_builder.hpp"

#include <cstring>

std::unique_ptr<Atom> build_tx3g_sample_entry() {
    auto tx3g = Atom::create("tx3g");
    auto &p = tx3g->payload;

    // Match the golden sample's tx3g entry bytes as closely as possible.
    // SampleEntry header (6 reserved + data_reference_index=1)
    write_u32(p, 0);
    write_u16(p, 0);
    write_u16(p, 1);

    // displayFlags
    write_u32(p, 0x00000000);
    // justification
    write_u8(p, 0x01);
    write_u8(p, 0xFF);
    // background color
    write_u8(p, 0x1f);
    write_u8(p, 0x1f);
    write_u8(p, 0x1f);
    write_u8(p, 0x00);
    // default text box
    write_u16(p, 0x0000);
    write_u16(p, 0x0000);
    write_u16(p, 0x0000);
    write_u16(p, 0x0000);
    // default style record (as seen in golden)
    write_u16(p, 0x0000);  // startChar
    write_u16(p, 0x0000);  // endChar
    write_u16(p, 0x0001);  // fontID
    write_u8(p, 0x01);     // font_face
    write_u8(p, 0x12);     // font_size
    write_u8(p, 0x00);     // text color RGBA
    write_u8(p, 0x00);
    write_u8(p, 0x00);
    write_u8(p, 0xFF);

    // Font table ('Sans-Serif', length 0x0a, no terminator)
    auto ftab = Atom::create("ftab");
    auto &f = ftab->payload;
    write_u16(f, 1);  // entry_count
    write_u16(f, 1);  // fontID
    const char fontName[] = "Sans-Serif";
    write_u8(f, 10);  // length
    f.insert(f.end(), fontName, fontName + 10);

    tx3g->add(std::move(ftab));

    return tx3g;
}
