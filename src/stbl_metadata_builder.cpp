//
//  stbl_metadata_builder.cpp.
//  ChapterForge.
//

#include "stbl_metadata_builder.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

#include "chapter_timing.hpp"
#include "mp4_atoms.hpp"

// Build stts from start times (durations in track timescale)
static std::unique_ptr<Atom> build_stts_meta(const std::vector<ChapterMetadataSample> &samples,
                                             uint32_t timescale) {
    auto stts = Atom::create("stts");
    auto &p = stts->payload;
    write_u32(p, 0);  // version/flags

    auto durations = derive_durations_ms_from_starts(samples);
    write_u32(p, static_cast<uint32_t>(durations.size()));
    for (auto dur_ms : durations) {
        uint32_t ts = (uint32_t)((dur_ms * timescale) / 1000);
        write_u32(p, 1);   // sample_count
        write_u32(p, ts);  // sample_delta
    }
    stts->fix_size_recursive();
    return stts;
}

// stsc: one chunk per sample
static std::unique_ptr<Atom> build_stsc_meta(const std::vector<ChapterMetadataSample> &samples) {
    auto stsc = Atom::create("stsc");
    auto &p = stsc->payload;
    write_u32(p, 0);  // version/flags
    write_u32(p, static_cast<uint32_t>(samples.size()));
    for (uint32_t i = 0; i < samples.size(); ++i) {
        write_u32(p, i + 1);  // first_chunk
        write_u32(p, 1);      // samples_per_chunk
        write_u32(p, 1);      // sample_description_index
    }
    stsc->fix_size_recursive();
    return stsc;
}

// stsz: sample sizes
static std::unique_ptr<Atom> build_stsz_meta(const std::vector<ChapterMetadataSample> &samples) {
    auto stsz = Atom::create("stsz");
    auto &p = stsz->payload;
    write_u32(p, 0);  // version/flags
    write_u32(p, 0);  // sample_size (0 = sizes follow)
    write_u32(p, static_cast<uint32_t>(samples.size()));
    for (const auto &s : samples) {
        write_u32(p, static_cast<uint32_t>(s.payload.size()));
    }
    stsz->fix_size_recursive();
    return stsz;
}

// stco placeholder; filled later
static std::unique_ptr<Atom> build_stco_meta(const std::vector<ChapterMetadataSample> &samples) {
    auto stco = Atom::create("stco");
    auto &p = stco->payload;
    write_u32(p, 0);  // version/flags
    write_u32(p, static_cast<uint32_t>(samples.size()));
    // placeholder zeros; patched later.
    for (size_t i = 0; i < samples.size(); ++i) {
        write_u32(p, 0);
    }
    stco->fix_size_recursive();
    return stco;
}

// stsd with a 'mett' sample entry (text/plain, UTF-8)
static std::unique_ptr<Atom> build_stsd_meta() {
    auto stsd = Atom::create("stsd");
    write_u32(stsd->payload, 0);  // version/flags
    write_u32(stsd->payload, 1);  // entry_count

    auto entry = Atom::create("mett");
    auto &p = entry->payload;
    // 6 reserved bytes
    write_u8(p, 0);
    write_u8(p, 0);
    write_u8(p, 0);
    write_u8(p, 0);
    write_u8(p, 0);
    write_u8(p, 0);
    write_u16(p, 1);  // data_reference_index

    // content_encoding (empty)
    write_u8(p, 0);
    // mime_format
    const char *mime = "text/plain";
    uint8_t mime_len = static_cast<uint8_t>(strlen(mime));
    write_u8(p, mime_len);
    p.insert(p.end(), mime, mime + mime_len);
    // metadata_namespace (empty)
    write_u8(p, 0);

    entry->fix_size_recursive();
    stsd->add(std::move(entry));
    stsd->fix_size_recursive();
    return stsd;
}

std::unique_ptr<Atom> build_metadata_stbl(const std::vector<ChapterMetadataSample> &samples,
                                          uint32_t track_timescale) {
    auto stbl = Atom::create("stbl");
    stbl->add(build_stsd_meta());
    stbl->add(build_stts_meta(samples, track_timescale));
    stbl->add(build_stsc_meta(samples));
    stbl->add(build_stsz_meta(samples));
    stbl->add(build_stco_meta(samples));
    stbl->fix_size_recursive();
    return stbl;
}
