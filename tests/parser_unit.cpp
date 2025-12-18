#include "parser.hpp"
#include "parser_test_utils.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using namespace parser_test_utils;

int main() {
    // Build a tiny MP4: ftyp + moov (single audio trak) + mdat placeholder.
    constexpr uint32_t kTimescale = 1000;
    constexpr uint32_t kDuration = 2000;
    constexpr uint32_t kSamples = 2;

    auto stbl_payload = std::vector<uint8_t>{};
    append_atom(stbl_payload, 'stsd', make_stsd());
    append_atom(stbl_payload, 'stts', make_stts(kSamples, 1024));
    append_atom(stbl_payload, 'stsc', make_stsc_empty());
    append_atom(stbl_payload, 'stsz', make_stsz(kSamples, 0));
    append_atom(stbl_payload, 'stco', make_stco_empty());

    auto mdia_payload = make_mdia(kTimescale, kDuration, stbl_payload, 'soun');
    auto trak_payload = make_trak(mdia_payload);
    auto moov_payload = make_moov(trak_payload);

    std::vector<uint8_t> file;
    // ftyp (ignored by parser).
    std::vector<uint8_t> ftyp_payload;
    ftyp_payload.insert(ftyp_payload.end(), {'i','s','o','m'});
    write_u32_be(ftyp_payload, 0);  // minor version
    ftyp_payload.insert(ftyp_payload.end(), {'i','s','o','m'});
    append_atom(file, 'ftyp', ftyp_payload);
    append_atom(file, 'moov', moov_payload);
    // minimal mdat
    std::vector<uint8_t> mdat_payload(4, 0);
    append_atom(file, 'mdat', mdat_payload);

    auto tmp = write_temp_file(file, "parser_unit.mp4");
    auto parsed = parse_mp4(tmp.string());
    assert(parsed.has_value());
    assert(parsed->audio_timescale == kTimescale);
    assert(parsed->audio_duration == kDuration);
    assert(parsed->stsz.size() >= 12);
    uint32_t parsed_samples =
        (parsed->stsz[8] << 24) | (parsed->stsz[9] << 16) | (parsed->stsz[10] << 8) |
        parsed->stsz[11];
    assert(parsed_samples == kSamples);

    // Clean up the temp file.
    std::filesystem::remove(tmp);
    return 0;
}
