//
//  mdia_builder.cpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#include "mdia_builder.hpp"

#include "hdlr_builder.hpp"
#include "mdhd_builder.hpp"
#include "minf_builder.hpp"

std::unique_ptr<Atom> build_mdia_audio(uint32_t timescale, uint64_t duration_ts,
                                       std::unique_ptr<Atom> stbl) {
    auto mdia = Atom::create("mdia");
    mdia->add(build_mdhd(timescale, duration_ts));
    mdia->add(build_hdlr_sound());
    mdia->add(build_minf_audio(std::move(stbl)));
    return mdia;
}

std::unique_ptr<Atom> build_mdia_text(uint32_t timescale, uint64_t duration_ts,
                                      std::unique_ptr<Atom> stbl) {
    auto mdia = Atom::create("mdia");
    mdia->add(build_mdhd(timescale, duration_ts));
    mdia->add(build_hdlr_text());
    mdia->add(build_minf_text(std::move(stbl)));
    return mdia;
}

std::unique_ptr<Atom> build_mdia_image(uint32_t timescale, uint64_t duration_ts,
                                       std::unique_ptr<Atom> stbl) {
    auto mdia = Atom::create("mdia");
    mdia->add(build_mdhd(timescale, duration_ts));
    mdia->add(build_hdlr_video());
    mdia->add(build_minf_image(std::move(stbl)));
    return mdia;
}
