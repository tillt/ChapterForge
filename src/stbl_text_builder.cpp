//
//  stbl_text_builder.cpp.
//  ChapterForge.
//
//  Created by Till Toenshoff on 12/9/25.
//

#include "stbl_text_builder.hpp"

#include <limits>

#include "chapter_timing.hpp"
#include "mp4_atoms.hpp"
#include "tx3g_stsd_builder.hpp"

// Encode a tx3g sample, optionally with an 'href' modifier (Apple chapter URLs).
static std::vector<uint8_t> encode_tx3g_sample(const ChapterTextSample &s) {
    std::vector<uint8_t> out;
    uint16_t len = static_cast<uint16_t>(s.text.size());
    write_u16(out, len);
    out.insert(out.end(), s.text.begin(), s.text.end());

    if (!s.href.empty()) {
        uint8_t url_len = static_cast<uint8_t>(std::min<size_t>(s.href.size(), 255));
        uint16_t start = 0;
        uint16_t end = 0x000a;  // golden sample href range
        uint32_t box_size = 4 + 4 + 2 + 2 + 1 + url_len + 1;  // +1 pad
        write_u32(out, box_size);
        out.push_back('h');
        out.push_back('r');
        out.push_back('e');
        out.push_back('f');
        write_u16(out, start);
        write_u16(out, end);
        out.push_back(url_len);
        out.insert(out.end(), s.href.begin(), s.href.begin() + url_len);
        out.push_back(0x00);  // pad
    }

    return out;
}

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
    // Emit one entry per sample (mirrors the golden chapter files).
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
    write_u32(p, 3);  // entry_count

    // Mirror golden: three entries, each 1 sample per chunk.
    write_u32(p, 1);  // first_chunk
    write_u32(p, 1);  // samples_per_chunk
    write_u32(p, 1);  // sample description index

    write_u32(p, 2);  // first_chunk
    write_u32(p, 1);  // samples_per_chunk
    write_u32(p, 1);

    write_u32(p, 3);  // first_chunk
    write_u32(p, 1);  // samples_per_chunk
    write_u32(p, 1);

    return stsc;
}

// -----------------------------------------------------------------------------
// stsz: byte size for each text sample.
// -----------------------------------------------------------------------------
static uint32_t encoded_tx3g_size(const ChapterTextSample &s) {
    uint32_t base = static_cast<uint32_t>(s.text.size() + 2);  // len(2) + text
    if (!s.href.empty()) {
        uint8_t url_len = static_cast<uint8_t>(std::min<size_t>(s.href.size(), 255));
        uint32_t href_box = 4 + 4 + 2 + 2 + 1 + url_len + 1;  // size+type+start+end+len+url+pad
        base += href_box;
    }
    return base;
}

static std::unique_ptr<Atom> build_stsz_text(const std::vector<ChapterTextSample> &samples) {
    auto stsz = Atom::create("stsz");
    auto &p = stsz->payload;

    write_u8(p, 0);
    write_u24(p, 0);
    write_u32(p, 0);  // variable sizes
    write_u32(p, samples.size());

    for (auto &s : samples) {
        write_u32(p, encoded_tx3g_size(s));
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
    // Apple-authored files carry extra trailing text samples (title/URL) after the last chapter.
    // Pad by duplicating the final sample twice so sample_count == chapter_count + 2.
    std::vector<ChapterTextSample> padded = samples;
    if (!padded.empty()) {
        padded.push_back(padded.back());
        padded.push_back(padded.back());
    }

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

    stbl->add(build_stts_text(padded, track_timescale));
    stbl->add(build_stsc_text());
    stbl->add(build_stsz_text(padded));
    stbl->add(build_stco_text(padded.size()));

    // Encode samples into an stsd sibling? (mdat payload handled elsewhere)

    return stbl;
}
