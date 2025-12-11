//
//  moov_builder.cpp.
//  ChapterForge.
//
//  Created by Till Toenshoff on 12/9/25.
//

#include "moov_builder.hpp"

#include "mdhd_builder.hpp"
#include "mvhd_builder.hpp"
#include "tkhd_builder.hpp"

std::unique_ptr<Atom> build_moov(
    uint32_t timescale, uint64_t duration_ts, std::unique_ptr<Atom> trak_audio,
    std::vector<std::unique_ptr<Atom>> text_tracks, std::unique_ptr<Atom> trak_image,
    std::unique_ptr<Atom> udta) {
    auto moov = Atom::create("moov");

    moov->add(build_mvhd(timescale, duration_ts));
    moov->add(std::move(trak_audio));
    for (auto &t : text_tracks) {
        moov->add(std::move(t));
    }
    if (trak_image) {
        moov->add(std::move(trak_image));
    }
    moov->add(std::move(udta));

    return moov;
}
