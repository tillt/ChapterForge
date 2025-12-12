//
//  hdlr_builder.cpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#include "hdlr_builder.hpp"

#include <cstring>

static std::unique_ptr<Atom> build_hdlr(const char type[4], const char *name) {
    auto h = Atom::create("hdlr");
    auto &p = h->payload;

    write_u8(p, 0);   // version
    write_u24(p, 0);  // flags

    write_u32(p, 0);             // pre_defined
    write_u32(p, fourcc(type));  // handler_type

    write_u32(p, 0);  // reserved[0]
    write_u32(p, 0);  // reserved[1]
    write_u32(p, 0);  // reserved[2]

    size_t len = strlen(name);
    p.insert(p.end(), name, name + len);
    p.push_back(0);  // NULL terminated string

    return h;
}

std::unique_ptr<Atom> build_hdlr_sound() { return build_hdlr("soun", "sound handler"); }

std::unique_ptr<Atom> build_hdlr_text(const std::string &name) {
    return build_hdlr("text", name.c_str());
}

std::unique_ptr<Atom> build_hdlr_metadata(const std::string &name) {
    // Timed metadata tracks use handler type 'meta'.
    return build_hdlr("meta", name.c_str());
}

std::unique_ptr<Atom> build_hdlr_video() { return build_hdlr("vide", "Chapter Images"); }
