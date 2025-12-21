//
//  meta_builder.cpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#include "meta_builder.hpp"

#include <cstring>
#include <vector>

#include "metadata_set.hpp"
#include "mp4_atoms.hpp"

// ILST item structure:
//
// [<fourcc>] atom.
//   [data] sub-atom.
//      type   (uint32)
//      lang   (uint32=0)
//      <raw data>

// Create a single ilst entry with a data atom of the given type.
std::unique_ptr<Atom> build_ilst_item(const std::string &fourcc, const std::vector<uint8_t> &data,
                                      uint32_t data_type) {
    auto item = Atom::create(fourcc.c_str());

    auto data_atom = Atom::create("data");
    auto &d = data_atom->payload;

    // data header.
    write_u32(d, data_type);  // type (1 = UTF-8, 13 = JPEG/PNG)
    write_u32(d, 0);          // locale/language = 0

    // value.
    d.insert(d.end(), data.begin(), data.end());

    item->add(std::move(data_atom));
    return item;
}


// Assemble an ilst atom from the supplied items.
std::unique_ptr<Atom> build_ilst(std::vector<std::unique_ptr<Atom>> items) {
    auto ilst = Atom::create("ilst");

    for (auto &item : items) {
        ilst->add(std::move(item));
    }

    return ilst;
}


// Wrap ilst in a meta atom containing the required handler.
std::unique_ptr<Atom> build_meta(std::unique_ptr<Atom> ilst) {
    auto meta = Atom::create("meta");

    auto &p = meta->payload;

    // FullBox header.
    write_u8(p, 0);   // version
    write_u24(p, 0);  // flags

    // Handler inside meta:
    auto hdlr = Atom::create("hdlr");
    auto &h = hdlr->payload;

    write_u8(h, 0);
    write_u24(h, 0);
    write_u32(h, 0);                           // pre_defined
    write_u32(h, fourcc('m', 'd', 'i', 'r'));  // handler_type
    write_u32(h, 0);                           // reserved[0]
    write_u32(h, 0);                           // reserved[1]
    write_u32(h, 0);                           // reserved[2]

    const char *name = "ilst handler";
    h.insert(h.end(), name, name + strlen(name));
    h.push_back(0);

    meta->add(std::move(hdlr));
    meta->add(std::move(ilst));

    return meta;
}

// Helper: write <data> atom payload.
static std::unique_ptr<Atom> build_data_atom(const std::vector<uint8_t> &bytes, uint32_t type = 1) {
    auto data = Atom::create("data");
    auto &p = data->payload;

    // type (4 bytes), locale (4 bytes)
    write_u32(p, type);
    write_u32(p, 0);

    // payload.
    p.insert(p.end(), bytes.begin(), bytes.end());
    return data;
}

static std::unique_ptr<Atom> build_string_item(const char *tag, const std::string &value) {
    auto item = Atom::create(tag);
    auto data = build_data_atom(std::vector<uint8_t>(value.begin(), value.end()));
    item->add(std::move(data));
    return item;
}

static std::unique_ptr<Atom> build_cover_item(const std::vector<uint8_t> &cover) {
    auto item = Atom::create("covr");
    // JPEG = type 13, PNG = type 14 (Apple metadata types)
    uint32_t type = 13;  // assume JPEG
    auto data = build_data_atom(cover, type);
    item->add(std::move(data));
    return item;
}

// Build udta/meta/ilst container (Apple-style)
// Build the full udta/meta/ilst structure for top-level metadata.
std::unique_ptr<Atom> build_meta_atom(const MetadataSet &meta) {
    //
    // ilst.
    //
    auto ilst = Atom::create("ilst");

    if (!meta.title.empty()) {
        ilst->add(
            build_string_item("\xA9"
                              "nam",
                              meta.title));
    }

    if (!meta.artist.empty()) {
        ilst->add(
            build_string_item("\xA9"
                              "ART",
                              meta.artist));
    }

    if (!meta.album.empty()) {
        ilst->add(
            build_string_item("\xA9"
                              "alb",
                              meta.album));
    }

    if (!meta.genre.empty()) {
        ilst->add(
            build_string_item("\xA9"
                              "gen",
                              meta.genre));
    }

    if (!meta.year.empty()) {
        ilst->add(
            build_string_item("\xA9"
                              "day",
                              meta.year));
    }

    if (!meta.comment.empty()) {
        ilst->add(
            build_string_item("\xA9"
                              "cmt",
                              meta.comment));
    }

    if (!meta.cover.empty()) {
        ilst->add(build_cover_item(meta.cover));
    }

    //
    // meta (fullbox)
    //
    auto meta_atom = Atom::create("meta");
    {
        // meta version+flags.
        meta_atom->payload.push_back(0);
        meta_atom->payload.push_back(0);
        meta_atom->payload.push_back(0);
        meta_atom->payload.push_back(0);

        // hdlr for metadata.
        auto hdlr = Atom::create("hdlr");
        auto &p = hdlr->payload;
        write_u8(p, 0);
        write_u24(p, 0);  // version+flags
        write_u32(p, 0);  // pre-defined

        // handler_type = 'mdir'
        p.push_back('m');
        p.push_back('d');
        p.push_back('i');
        p.push_back('r');

        write_u32(p, 0);  // reserved
        write_u32(p, 0);  // reserved

        // name string (null-terminated)
        const char name[] = "ilst handler";
        p.insert(p.end(), name, name + strlen(name));
        p.push_back(0);

        meta_atom->add(std::move(hdlr));

        // Now add ilst under meta.
        meta_atom->add(std::move(ilst));
    }

    return meta_atom;
}

// Build meta atom using an existing ilst payload.
std::unique_ptr<Atom> build_meta_atom_from_ilst(const std::vector<uint8_t> &ilst_payload) {
    auto ilst = Atom::create("ilst");
    ilst->payload = ilst_payload;

    // meta (fullbox)
    auto meta_atom = Atom::create("meta");
    meta_atom->payload = {0, 0, 0, 0};  // version+flags

    auto hdlr = Atom::create("hdlr");
    auto &p = hdlr->payload;
    write_u8(p, 0);
    write_u24(p, 0);  // version+flags
    write_u32(p, 0);  // pre-defined
    p.push_back('m');
    p.push_back('d');
    p.push_back('i');
    p.push_back('r');
    write_u32(p, 0);
    write_u32(p, 0);
    const char name[] = "ilst handler";
    p.insert(p.end(), name, name + strlen(name));
    p.push_back(0);

    meta_atom->add(std::move(hdlr));
    meta_atom->add(std::move(ilst));
    return meta_atom;
}
