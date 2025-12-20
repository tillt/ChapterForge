//
//  stbl_image_builder.cpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#include "stbl_image_builder.hpp"

#include "chapter_timing.hpp"
#include "jpeg_entry_builder.hpp"

// stts (same logic as text track)
static std::unique_ptr<Atom> build_stts_img(const std::vector<ChapterImageSample> &samples,
                                            uint32_t timescale, uint32_t total_ms) {
    auto stts = Atom::create("stts");
    auto &p = stts->payload;

    write_u8(p, 0);
    write_u24(p, 0);
    auto durations = derive_durations_ms_from_starts(samples, total_ms);
    // Emit one entry per sample (avoids RLE collapsing; closer to golden files).
    write_u32(p, durations.size());
    for (auto dur_ms : durations) {
        uint32_t dur = (uint64_t)dur_ms * timescale / 1000;
        write_u32(p, 1);    // sample count
        write_u32(p, dur);  // sample duration
    }

    return stts;
}

// stss: mark every sample as sync (key) sample.
static std::unique_ptr<Atom> build_stss_img(uint32_t sample_count) {
    auto stss = Atom::create("stss");
    auto &p = stss->payload;

    write_u8(p, 0);
    write_u24(p, 0);
    write_u32(p, sample_count);
    for (uint32_t i = 0; i < sample_count; ++i) {
        write_u32(p, i + 1);  // 1-based sample numbers
    }
    return stss;
}

// stsc: 1 sample per chunk.
static std::unique_ptr<Atom> build_stsc_img(const std::vector<uint32_t> &chunk_plan) {
    auto stsc = Atom::create("stsc");
    auto &p = stsc->payload;

    write_u8(p, 0);
    write_u24(p, 0);
    if (chunk_plan.empty()) {
        write_u32(p, 1);
        write_u32(p, 1);
        write_u32(p, 1);
        write_u32(p, 1);
        return stsc;
    }

    // Build run-length entries for changes in samples_per_chunk.
    struct Entry {
        uint32_t first_chunk;
        uint32_t samples_per_chunk;
    };
    std::vector<Entry> entries;
    uint32_t chunk_index = 1;
    uint32_t current = chunk_plan.front();
    entries.push_back({chunk_index, current});
    for (size_t i = 1; i < chunk_plan.size(); ++i) {
        if (chunk_plan[i] != current) {
            current = chunk_plan[i];
            entries.push_back({static_cast<uint32_t>(i + 1), current});
        }
    }

    write_u32(p, static_cast<uint32_t>(entries.size()));
    for (auto &e : entries) {
        write_u32(p, e.first_chunk);
        write_u32(p, e.samples_per_chunk);
        write_u32(p, 1);  // sample_desc_index
    }

    return stsc;
}

// stsz: JPEG sizes.
static std::unique_ptr<Atom> build_stsz_img(const std::vector<ChapterImageSample> &samples) {
    auto stsz = Atom::create("stsz");
    auto &p = stsz->payload;

    write_u8(p, 0);
    write_u24(p, 0);

    write_u32(p, 0);  // variable size
    write_u32(p, samples.size());

    for (auto &s : samples) {
        write_u32(p, s.data.size());
    }

    return stsz;
}

// stco: patched later.
static std::unique_ptr<Atom> build_stco_img(uint32_t count) {
    auto stco = Atom::create("stco");
    auto &p = stco->payload;

    write_u8(p, 0);
    write_u24(p, 0);
    write_u32(p, count);

    for (uint32_t i = 0; i < count; ++i) {
        write_u32(p, 0);
    }

    return stco;
}

// Build image stbl (jpeg sample entry + timing + chunk tables).
std::unique_ptr<Atom> build_image_stbl(const std::vector<ChapterImageSample> &samples,
                                       uint32_t track_timescale, uint16_t width, uint16_t height,
                                       const std::vector<uint32_t> &chunk_plan,
                                       uint32_t total_ms) {
    auto stbl = Atom::create("stbl");

    stbl->add(build_stsd_jpeg(width, height));  // sample entry with display size
    stbl->add(build_stts_img(samples, track_timescale, total_ms));
    stbl->add(build_stss_img(static_cast<uint32_t>(samples.size())));
    stbl->add(build_stsc_img(chunk_plan));
    stbl->add(build_stsz_img(samples));
    uint32_t chunk_count = chunk_plan.empty() ? static_cast<uint32_t>(samples.size())
                                              : static_cast<uint32_t>(chunk_plan.size());
    stbl->add(build_stco_img(chunk_count));

    return stbl;
}
