#include "parser.hpp"
#include "parser_test_utils.hpp"

#include <cassert>
#include <cstdint>
#include <sstream>
#include <vector>

using parser_detail::TrackParseResult;
using namespace parser_test_utils;

int main() {
    // Happy-path stbl/trak parse.
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
    std::string trak_str(trak_buf.begin() + 8, trak_buf.end());  // drop outer header for payload
    std::istringstream trak_stream(trak_str);
    bool fallback = false;
    auto track = parse_trak_for_test(trak_stream, trak_buf.size() - 8,
                                     trak_buf.size(), fallback);
    assert(track.has_value());
    assert(!fallback);
    assert(track->handler_type == 'soun');
    assert(track->timescale == timescale);
    assert(track->duration == duration);
    uint32_t parsed_samples =
        (track->stsz[8] << 24) | (track->stsz[9] << 16) | (track->stsz[10] << 8) |
        track->stsz[11];
    assert(parsed_samples == samples);

    // Malformed atom should trigger fallback.
    std::vector<uint8_t> bad_trak;
    append_atom(bad_trak, 'trak', std::vector<uint8_t>{0x00});  // too small payload
    std::string bad_str(bad_trak.begin() + 8, bad_trak.end());
    std::istringstream bad_stream(bad_str);
    fallback = false;
    auto bad = parse_trak_for_test(bad_stream, bad_trak.size() - 8,
                                   bad_trak.size(), fallback);
    assert(!bad.has_value() || fallback);

    // Basic moov parse wrapper with a single trak.
    std::vector<uint8_t> moov_payload;
    append_atom(moov_payload, 'mvhd', std::vector<uint8_t>(100, 0));
    append_atom(moov_payload, 'trak', make_trak(mdia));

    std::vector<uint8_t> file;
    append_atom(file, 'moov', moov_payload);
    std::string moov_str(file.begin() + 8, file.end());
    std::istringstream moov_stream(moov_str);
    Mp4AtomInfo atom{};
    atom.type = 'moov';
    atom.offset = 0;
    atom.size = static_cast<uint64_t>(file.size());
    ParsedMp4 parsed{};
    uint32_t best_samples = 0;
    fallback = false;
    parse_moov_for_test(moov_stream, atom, file.size(), parsed, best_samples, fallback);
    assert(!fallback);
    assert(parsed.audio_timescale == timescale);
    assert(parsed.audio_duration == duration);
    assert(best_samples == samples);

    return 0;
}
