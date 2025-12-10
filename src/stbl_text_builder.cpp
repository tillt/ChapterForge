//
//  stbl_text_builder.cpp.
//  ChapterForge.
//
//  Created by Till Toenshoff on 12/9/25.
//

#include "stbl_text_builder.hpp"

#include "chapter_timing.hpp"
#include "mp4_atoms.hpp"
#include "tx3g_stsd_builder.hpp"

// -----------------------------------------------------------------------------
// stts: durations converted to track timescale.
// -----------------------------------------------------------------------------
static std::unique_ptr<Atom> build_stts_text(const std::vector<ChapterTextSample> &samples,
                                             uint32_t timescale) {
    auto stts = Atom::create("stts");
    auto &p = stts->payload;

    write_u8(p, 0);
    write_u24(p, 0);

    auto durations = derive_durations_ms_from_starts(samples);
    write_u32(p, durations.size());

    for (auto dur_ms : durations) {
        uint32_t dur = (uint64_t)dur_ms * timescale / 1000;
        write_u32(p, 1);    // sample count
        write_u32(p, dur);  // sample duration
    }

    return stts;
}

// -----------------------------------------------------------------------------
// stsc: 1 sample per chunk.
// -----------------------------------------------------------------------------
static std::unique_ptr<Atom> build_stsc_text() {
    auto stsc = Atom::create("stsc");
    auto &p = stsc->payload;

    write_u8(p, 0);
    write_u24(p, 0);
    write_u32(p, 1);

    write_u32(p, 1);  // first_chunk
    write_u32(p, 1);  // samples_per_chunk
    write_u32(p, 1);  // sample description index

    return stsc;
}

// -----------------------------------------------------------------------------
// stsz: byte size for each text sample.
// -----------------------------------------------------------------------------
static std::unique_ptr<Atom> build_stsz_text(const std::vector<ChapterTextSample> &samples) {
    auto stsz = Atom::create("stsz");
    auto &p = stsz->payload;

    auto encoded_size = [](const ChapterTextSample &s) -> uint32_t {
        // tx3g sample = 2-byte length + text bytes + 2-byte style count (zero)
        return static_cast<uint32_t>(s.text.size() + 4);
    };

    write_u8(p, 0);
    write_u24(p, 0);
    write_u32(p, 0);  // variable sizes
    write_u32(p, samples.size());

    for (auto &s : samples) {
        write_u32(p, encoded_size(s));
    }

    return stsz;
}

// -----------------------------------------------------------------------------
// stco: 1 entry per sample (patched later)
// -----------------------------------------------------------------------------
static std::unique_ptr<Atom> build_stco_text(uint32_t count) {
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

// -----------------------------------------------------------------------------
// Build text chapter stbl.
// -----------------------------------------------------------------------------
std::unique_ptr<Atom> build_text_stbl(const std::vector<ChapterTextSample> &samples,
                                      uint32_t track_timescale) {
    auto stbl = Atom::create("stbl");

    // stsd wrapper with a single tx3g sample entry.
    {
        auto stsd = Atom::create("stsd");
        auto &p = stsd->payload;

        write_u8(p, 0);
        write_u24(p, 0);  // version+flags
        write_u32(p, 1);  // entry_count = 1
        stsd->add(build_tx3g_sample_entry());

        stbl->add(std::move(stsd));
    }

    stbl->add(build_stts_text(samples, track_timescale));
    stbl->add(build_stsc_text());
    stbl->add(build_stsz_text(samples));
    stbl->add(build_stco_text(samples.size()));

    return stbl;
}
