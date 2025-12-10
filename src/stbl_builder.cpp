//
//  stbl_builder.cpp.
//  ChapterForge.
//
//  Created by Till Toenshoff on 12/9/25.
//

#include "stbl_builder.hpp"

#include "jpeg_entry_builder.hpp"
#include "stco_builder.hpp"
#include "stsc_builder.hpp"
#include "stsz_builder.hpp"
#include "stts_builder.hpp"
#include "tx3g_stsd_builder.hpp"

std::unique_ptr<Atom> build_text_stbl(const std::vector<uint32_t> &timestamps_ts,
                                      uint32_t total_duration_ts,
                                      const std::vector<uint32_t> &sample_sizes) {
    auto stbl = Atom::create("stbl");

    // --- 1) stsd ---
    auto stsd = Atom::create("stsd");
    auto &sd = stsd->payload;

    write_u8(sd, 0);   // version
    write_u24(sd, 0);  // flags
    write_u32(sd, 1);  // entry count = 1

    stsd->add(build_tx3g_sample_entry());
    stbl->add(std::move(stsd));

    // --- 2) stts ---
    stbl->add(build_stts(timestamps_ts, total_duration_ts));

    // --- 3) stsc ---
    stbl->add(build_stsc(sample_sizes.size()));

    // --- 4) stsz ---
    stbl->add(build_stsz(sample_sizes));

    // --- 5) stco ---
    stbl->add(build_stco());

    return stbl;
}

std::unique_ptr<Atom> build_image_stbl(const std::vector<uint32_t> &timestamps_ts,
                                       uint32_t total_duration_ts,
                                       const std::vector<uint32_t> &sample_sizes, uint16_t width,
                                       uint16_t height) {
    auto stbl = Atom::create("stbl");

    // --- 1) stsd ---
    auto stsd = Atom::create("stsd");
    auto &sd = stsd->payload;

    write_u8(sd, 0);   // version
    write_u24(sd, 0);  // flags
    write_u32(sd, 1);  // entry count = 1

    stsd->add(build_jpeg_sample_entry(width, height));
    stbl->add(std::move(stsd));

    // --- 2) stts ---
    stbl->add(build_stts(timestamps_ts, total_duration_ts));

    // --- 3) stsc ---
    stbl->add(build_stsc(sample_sizes.size()));

    // --- 4) stsz ---
    stbl->add(build_stsz(sample_sizes));

    // --- 5) stco ---
    stbl->add(build_stco());

    return stbl;
}
