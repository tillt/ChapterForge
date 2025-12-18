#include "parser.hpp"
#include "parser_test_utils.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <sstream>
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

    // Happy-path trak parse via test hook.
    {
        uint32_t timescale = 1000;
        uint32_t duration = 3000;
        uint32_t samples = 3;
        std::vector<uint8_t> stbl;
        append_atom(stbl, 'stsd', make_stsd_empty());
        append_atom(stbl, 'stts', make_stts_single(samples, 512));
        append_atom(stbl, 'stsc', make_stsc_empty());
        append_atom(stbl, 'stsz', make_stsz(samples, 0));
        append_atom(stbl, 'stco', make_stco_empty());

        auto mdia = make_mdia(timescale, duration, stbl, 'soun');
        std::vector<uint8_t> trak_buf;
        append_atom(trak_buf, 'trak', make_trak(mdia));
        std::string trak_str(trak_buf.begin() + 8, trak_buf.end());  // drop outer header
        std::istringstream trak_stream(trak_str);
        bool fallback = false;
        auto track = parse_trak_for_test(trak_stream, trak_buf.size() - 8,
                                         trak_buf.size(), fallback);
        assert(track.has_value());
        assert(!fallback);
        assert(track->handler_type == 'soun');
        assert(track->timescale == timescale);
        assert(track->duration == duration);
        uint32_t parsed_track_samples =
            (track->stsz[8] << 24) | (track->stsz[9] << 16) | (track->stsz[10] << 8) |
            track->stsz[11];
        assert(parsed_track_samples == samples);
    }

    // Malformed trak should force fallback.
    {
        std::vector<uint8_t> bad_trak;
        append_atom(bad_trak, 'trak', std::vector<uint8_t>{0x00});  // too small payload
        std::string bad_str(bad_trak.begin() + 8, bad_trak.end());
        std::istringstream bad_stream(bad_str);
        bool fallback = false;
        auto bad = parse_trak_for_test(bad_stream, bad_trak.size() - 8,
                                       bad_trak.size(), fallback);
        assert(!bad.has_value() || fallback);
    }

    // Basic moov parse wrapper with a single trak.
    {
        uint32_t timescale = 1000;
        uint32_t duration = 3000;
        uint32_t samples = 3;

        std::vector<uint8_t> stbl;
        append_atom(stbl, 'stsd', make_stsd_empty());
        append_atom(stbl, 'stts', make_stts_single(samples, 512));
        append_atom(stbl, 'stsc', make_stsc_empty());
        append_atom(stbl, 'stsz', make_stsz(samples, 0));
        append_atom(stbl, 'stco', make_stco_empty());

        auto mdia = make_mdia(timescale, duration, stbl, 'soun');

        std::vector<uint8_t> moov_payload;
        append_atom(moov_payload, 'mvhd', std::vector<uint8_t>(100, 0));
        append_atom(moov_payload, 'trak', make_trak(mdia));

        std::vector<uint8_t> file2;
        append_atom(file2, 'moov', moov_payload);
        std::string moov_str(file2.begin() + 8, file2.end());
        std::istringstream moov_stream(moov_str);
        Mp4AtomInfo atom{};
        atom.type = 'moov';
        atom.offset = 0;
        atom.size = static_cast<uint64_t>(file2.size());
        ParsedMp4 parsed2{};
        uint32_t best_samples = 0;
        bool fallback = false;
        parse_moov_for_test(moov_stream, atom, file2.size(), parsed2, best_samples, fallback);
        assert(!fallback);
        assert(parsed2.audio_timescale == timescale);
        assert(parsed2.audio_duration == duration);
        assert(best_samples == samples);
    }

    // Clean up the temp file.
    std::filesystem::remove(tmp);
    return 0;
}
