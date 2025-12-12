//
//  minf_builder.cpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#include "minf_builder.hpp"

#include "dinf_builder.hpp"
#include "hdlr_builder.hpp"
#include "nmhd_builder.hpp"
#include "smhd_builder.hpp"
#include "vmhd_builder.hpp"

// Audio: smhd + dinf + stbl.
std::unique_ptr<Atom> build_minf_audio(std::unique_ptr<Atom> stbl) {
    auto minf = Atom::create("minf");
    minf->add(build_smhd());
    minf->add(build_dinf());
    minf->add(std::move(stbl));
    return minf;
}

// Text: nmhd + dinf + stbl.
std::unique_ptr<Atom> build_minf_text(std::unique_ptr<Atom> stbl) {
    auto minf = Atom::create("minf");
    minf->add(build_nmhd());
    minf->add(build_dinf());
    minf->add(std::move(stbl));
    return minf;
}

// Image: vmhd + dinf + stbl.
std::unique_ptr<Atom> build_minf_image(std::unique_ptr<Atom> stbl) {
    auto minf = Atom::create("minf");
    minf->add(build_vmhd());
    minf->add(build_dinf());
    minf->add(std::move(stbl));
    return minf;
}
