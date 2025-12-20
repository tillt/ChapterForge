//
//  stbl_audio_builder.cpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#include "stbl_audio_builder.hpp"

#include <tuple>

#include "stsd_builder.hpp"

// Build stts: each AAC frame = 1024 PCM samples in AAC-LC.
static std::unique_ptr<Atom> build_stts(uint32_t sample_count) {
    auto stts = Atom::create("stts");
    auto &p = stts->payload;

    write_u8(p, 0);
    write_u24(p, 0);  // version+flags

    // entry_count = 1.
    write_u32(p, 1);

    // each frame = 1024 ticks.
    write_u32(p, sample_count);
    write_u32(p, 1024);

    return stts;
}

// Build stsc from a chunk plan.
static std::unique_ptr<Atom> build_stsc(const std::vector<uint32_t> &chunk_sizes) {
    auto stsc = Atom::create("stsc");
    auto &p = stsc->payload;

    write_u8(p, 0);
    write_u24(p, 0);  // version+flags

    // Run-length encode chunks with identical samples_per_chunk.
    std::vector<std::tuple<uint32_t, uint32_t>> entries;  // first_chunk, samples_per_chunk
    uint32_t chunk_index = 1;
    size_t i = 0;
    while (i < chunk_sizes.size()) {
        uint32_t samples = chunk_sizes[i];
        size_t run_start = i;
        while (i < chunk_sizes.size() && chunk_sizes[i] == samples) {
            ++i;
        }
        entries.emplace_back(chunk_index, samples);
        chunk_index += static_cast<uint32_t>(i - run_start);
    }

    write_u32(p, static_cast<uint32_t>(entries.size()));
    for (auto &e : entries) {
        write_u32(p, std::get<0>(e));  // first_chunk (1-based)
        write_u32(p, std::get<1>(e));  // samples_per_chunk
        write_u32(p, 1);               // sample_desc_index
    }

    return stsc;
}

// Build stsz (sizes of all AAC frames)
static std::unique_ptr<Atom> build_stsz(const std::vector<uint32_t> &sizes) {
    auto stsz = Atom::create("stsz");
    auto &p = stsz->payload;

    write_u8(p, 0);
    write_u24(p, 0);
    write_u32(p, 0);  // sample_size = 0 (variable)
    write_u32(p, sizes.size());

    for (uint32_t s : sizes) {
        write_u32(p, s);
    }

    return stsz;
}

// Build stco (initially empty — patched later)
static std::unique_ptr<Atom> build_stco(uint32_t chunk_count) {
    auto stco = Atom::create("stco");
    auto &p = stco->payload;

    write_u8(p, 0);
    write_u24(p, 0);

    write_u32(p, chunk_count);
    for (uint32_t i = 0; i < chunk_count; ++i) {
        write_u32(p, 0);  // filled later in patch_stco()
    }

    return stco;
}

// Build audio stbl from parsed AAC config and sample layout.
std::unique_ptr<Atom> build_audio_stbl(const Mp4aConfig &cfg,
                                       const std::vector<uint32_t> &sample_sizes,
                                       const std::vector<uint32_t> &chunk_sizes,
                                       uint32_t num_samples, const std::vector<uint8_t> *raw_stsd) {
    auto stbl = Atom::create("stbl");
    uint32_t chunk_count = static_cast<uint32_t>(chunk_sizes.size());

    if (raw_stsd && !raw_stsd->empty()) {
        auto stsd = Atom::create("stsd");
        stsd->payload = *raw_stsd;
        stbl->add(std::move(stsd));
    } else {
        stbl->add(build_stsd_mp4a(cfg));
    }
    stbl->add(build_stts(num_samples));
    stbl->add(build_stsc(chunk_sizes));
    stbl->add(build_stsz(sample_sizes));
    stbl->add(build_stco(chunk_count));

    return stbl;
}

// Rehydrate an audio stbl from raw atom payloads (used when reusing source stbl).
std::unique_ptr<Atom> build_audio_stbl_raw(const std::vector<uint8_t> &stsd_payload,
                                           const std::vector<uint8_t> &stts_payload,
                                           const std::vector<uint8_t> &stsc_payload,
                                           const std::vector<uint8_t> &stsz_payload,
                                           const std::vector<uint8_t> &stco_payload) {
    auto stbl = Atom::create("stbl");

    auto stsd = Atom::create("stsd");
    stsd->payload = stsd_payload;
    stbl->add(std::move(stsd));

    auto stts = Atom::create("stts");
    stts->payload = stts_payload;
    stbl->add(std::move(stts));

    auto stsc = Atom::create("stsc");
    stsc->payload = stsc_payload;
    stbl->add(std::move(stsc));

    auto stsz = Atom::create("stsz");
    stsz->payload = stsz_payload;
    stbl->add(std::move(stsz));

    auto stco = Atom::create("stco");
    stco->payload = stco_payload;
    stbl->add(std::move(stco));

    return stbl;
}
