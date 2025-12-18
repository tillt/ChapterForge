//
//  parser_test_utils.hpp
//  ChapterForge
//
//  Test-only helpers to build tiny MP4 atoms for parser unit tests.
//

#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace parser_test_utils {

inline void write_u32_be(std::vector<uint8_t> &buf, uint32_t v) {
    buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
}

inline void write_u64_be(std::vector<uint8_t> &buf, uint64_t v) {
    write_u32_be(buf, static_cast<uint32_t>((v >> 32) & 0xFFFFFFFF));
    write_u32_be(buf, static_cast<uint32_t>(v & 0xFFFFFFFF));
}

inline void append_atom(std::vector<uint8_t> &buf, uint32_t type,
                        const std::vector<uint8_t> &payload) {
    uint32_t size = static_cast<uint32_t>(payload.size() + 8);
    write_u32_be(buf, size);
    write_u32_be(buf, type);
    buf.insert(buf.end(), payload.begin(), payload.end());
}

inline std::vector<uint8_t> make_mdhd(uint32_t timescale, uint32_t duration) {
    std::vector<uint8_t> p;
    p.reserve(24);
    p.push_back(0); p.push_back(0); p.push_back(0); p.push_back(0);  // version/flags
    write_u32_be(p, 0);      // creation
    write_u32_be(p, 0);      // modification
    write_u32_be(p, timescale);
    write_u32_be(p, duration);
    write_u32_be(p, 0);      // language + pre-defined
    return p;
}

inline std::vector<uint8_t> make_hdlr(uint32_t handler_type) {
    std::vector<uint8_t> p;
    p.reserve(24);
    p.push_back(0); p.push_back(0); p.push_back(0); p.push_back(0);  // version/flags
    write_u32_be(p, 0);       // pre_defined
    write_u32_be(p, handler_type);
    write_u32_be(p, 0);       // reserved[0]
    write_u32_be(p, 0);       // reserved[1]
    write_u32_be(p, 0);       // reserved[2]
    p.push_back(0);           // empty name
    return p;
}

inline std::vector<uint8_t> make_stsd(uint32_t entry_count = 1) {
    std::vector<uint8_t> entry;
    // simple mp4a entry, 16 bytes total (size+type+8 reserved bytes)
    write_u32_be(entry, 16);
    write_u32_be(entry, 'mp4a');
    write_u32_be(entry, 0);
    write_u32_be(entry, 0);

    std::vector<uint8_t> p;
    p.reserve(24);
    p.push_back(0); p.push_back(0); p.push_back(0); p.push_back(0);  // version/flags
    write_u32_be(p, entry_count);  // entry_count
    for (uint32_t i = 0; i < entry_count; ++i) {
        p.insert(p.end(), entry.begin(), entry.end());
    }
    return p;
}

inline std::vector<uint8_t> make_stsd_empty() {
    std::vector<uint8_t> p;
    p.push_back(0); p.push_back(0); p.push_back(0); p.push_back(0);  // version/flags
    write_u32_be(p, 0);  // entry_count
    return p;
}

inline std::vector<uint8_t> make_stts_single(uint32_t count, uint32_t delta) {
    std::vector<uint8_t> p;
    p.push_back(0); p.push_back(0); p.push_back(0); p.push_back(0);  // version/flags
    write_u32_be(p, 1);        // entry_count
    write_u32_be(p, count);    // sample_count
    write_u32_be(p, delta);    // sample_delta
    return p;
}

inline std::vector<uint8_t> make_stts(uint32_t sample_count, uint32_t delta) {
    return make_stts_single(sample_count, delta);
}

inline std::vector<uint8_t> make_stsc_empty() {
    std::vector<uint8_t> p;
    p.reserve(8);
    p.push_back(0); p.push_back(0); p.push_back(0); p.push_back(0);  // version/flags
    write_u32_be(p, 0);  // entry_count
    return p;
}

inline std::vector<uint8_t> make_stco_empty() {
    std::vector<uint8_t> p;
    p.reserve(8);
    p.push_back(0); p.push_back(0); p.push_back(0); p.push_back(0);  // version/flags
    write_u32_be(p, 0);  // entry_count
    return p;
}

inline std::vector<uint8_t> make_stsz(uint32_t sample_count, uint32_t size_per_sample) {
    std::vector<uint8_t> p;
    p.push_back(0); p.push_back(0); p.push_back(0); p.push_back(0);  // version/flags
    write_u32_be(p, size_per_sample);  // sample_size (0 => entries follow)
    write_u32_be(p, sample_count);
    for (uint32_t i = 0; i < sample_count; ++i) {
        write_u32_be(p, 1);
    }
    return p;
}

inline std::vector<uint8_t> make_smhd() {
    std::vector<uint8_t> p;
    p.reserve(8);
    p.push_back(0); p.push_back(0); p.push_back(0); p.push_back(0);  // version/flags
    write_u32_be(p, 0);  // balance + reserved
    return p;
}

inline std::vector<uint8_t> make_minf(const std::vector<uint8_t> &stbl_payload) {
    std::vector<uint8_t> minf_payload;
    append_atom(minf_payload, 'smhd', make_smhd());
    // minimal dinf with empty dref (parser skips it).
    std::vector<uint8_t> dref;
    dref.push_back(0); dref.push_back(0); dref.push_back(0); dref.push_back(0);  // version/flags
    write_u32_be(dref, 0);  // entry_count
    std::vector<uint8_t> dinf_payload;
    append_atom(dinf_payload, 'dref', dref);
    append_atom(minf_payload, 'dinf', dinf_payload);
    append_atom(minf_payload, 'stbl', stbl_payload);
    return minf_payload;
}

inline std::vector<uint8_t> make_mdia(uint32_t timescale, uint32_t duration,
                                      const std::vector<uint8_t> &stbl_payload,
                                      uint32_t handler) {
    std::vector<uint8_t> payload;
    append_atom(payload, 'mdhd', make_mdhd(timescale, duration));
    append_atom(payload, 'hdlr', make_hdlr(handler));
    append_atom(payload, 'minf', make_minf(stbl_payload));
    return payload;
}

inline std::vector<uint8_t> make_trak(const std::vector<uint8_t> &mdia_payload) {
    std::vector<uint8_t> payload;
    std::vector<uint8_t> tkhd(84, 0);  // minimal tkhd
    append_atom(payload, 'tkhd', tkhd);
    append_atom(payload, 'mdia', mdia_payload);
    return payload;
}

inline std::vector<uint8_t> make_moov(const std::vector<uint8_t> &trak_payload) {
    std::vector<uint8_t> payload;
    std::vector<uint8_t> mvhd(100, 0);  // minimal mvhd
    append_atom(payload, 'mvhd', mvhd);
    append_atom(payload, 'trak', trak_payload);
    return payload;
}

inline std::filesystem::path write_temp_file(const std::vector<uint8_t> &data,
                                             const std::string &name) {
    auto tmp = std::filesystem::temp_directory_path() / name;
    std::ofstream out(tmp, std::ios::binary);
    out.write(reinterpret_cast<const char *>(data.data()),
              static_cast<std::streamsize>(data.size()));
    return tmp;
}

}  // namespace parser_test_utils

