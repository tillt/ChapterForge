//
//  parser.cpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#include "parser.hpp"

#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <chrono>

#include "logging.hpp"
#include "mp4_atoms.hpp"

namespace {

using parser_detail::TrackParseResult;

constexpr uint64_t kAtomHeaderSize = 8;
constexpr uint64_t kFullBoxBaseHeader = 12;  // size+type+version/flags
constexpr uint64_t kMetaReservedBytes = 4;
constexpr uint64_t kHdlrMinPayload = 20;
constexpr uint64_t kMaxAtomPayload = 512 * 1024 * 1024;  // 512 MB safety bound

static bool is_printable_fourcc(uint32_t type) {
    for (int i = 0; i < 4; ++i) {
        uint8_t c = (type >> (24 - 8 * i)) & 0xFF;
        if (c < 0x20 || c > 0x7E) {
            return false;
        }
    }
    return true;
}

static std::string fourcc_to_string(uint32_t type) {
    std::string s(4, ' ');
    s[0] = static_cast<char>((type >> 24) & 0xFF);
    s[1] = static_cast<char>((type >> 16) & 0xFF);
    s[2] = static_cast<char>((type >> 8) & 0xFF);
    s[3] = static_cast<char>(type & 0xFF);
    return s;
}

}  // namespace

static void skip(std::istream &in, uint64_t n) { in.seekg(n, std::ios::cur); }

uint32_t read_u32(std::istream &in) {
    uint8_t b[4];
    in.read(reinterpret_cast<char *>(b), 4);
    return (uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) | (uint32_t(b[2]) << 8) |
           (uint32_t(b[3]));
}

uint64_t read_u64(std::istream &in) {
    uint8_t b[8];
    in.read(reinterpret_cast<char *>(b), 8);
    return (uint64_t(b[0]) << 56) | (uint64_t(b[1]) << 48) | (uint64_t(b[2]) << 40) |
           (uint64_t(b[3]) << 32) | (uint64_t(b[4]) << 24) | (uint64_t(b[5]) << 16) |
           (uint64_t(b[6]) << 8) | (uint64_t(b[7]));
}

// Read atom header: size + type.
static Mp4AtomInfo read_atom_header(std::istream &in) {
    Mp4AtomInfo info;
    info.offset = in.tellg();
    info.size = read_u32(in);
    info.type = read_u32(in);

    if (info.size == 1) {
        // 64-bit extended size.
        info.size = read_u64(in);
    }
    // Hard sanity: reject absurd payloads early (protect against corrupted headers).
    if (info.size >= kAtomHeaderSize &&
        (info.size - kAtomHeaderSize) > kMaxAtomPayload) {
        CH_LOG("warn", "atom " << fourcc_to_string(info.type)
                               << " claims payload " << (info.size - kAtomHeaderSize)
                               << " bytes; exceeds safety bound, skipping");
        info.size = 0;
    }
    return info;
}

// Utility: read an atom payload into a byte buffer.
static std::vector<uint8_t> read_bytes(std::istream &in, uint64_t size) {
    std::vector<uint8_t> buf(size);
    in.read(reinterpret_cast<char *>(buf.data()), size);
    return buf;
}


static bool grab_atom_from_buffer(const std::vector<uint8_t> &buf, const char *fourcc,
                                  std::vector<uint8_t> &dst) {
    if (!dst.empty()) {
        return false;
    }
    for (size_t i = 0; i + 8 <= buf.size(); ++i) {
        if (buf[i + 4] == static_cast<uint8_t>(fourcc[0]) &&
            buf[i + 5] == static_cast<uint8_t>(fourcc[1]) &&
            buf[i + 6] == static_cast<uint8_t>(fourcc[2]) &&
            buf[i + 7] == static_cast<uint8_t>(fourcc[3])) {
            uint32_t sz = (buf[i] << 24) | (buf[i + 1] << 16) | (buf[i + 2] << 8) | buf[i + 3];
            uint64_t end = static_cast<uint64_t>(i) + static_cast<uint64_t>(sz);
            if (sz >= 8 && end <= buf.size() && end >= i + 8) {
                dst.assign(buf.begin() + i + 8, buf.begin() + end);
                CH_LOG("debug", "grabbed " << fourcc << " via raw scan, bytes=" << dst.size());
                return true;
            }
        }
    }
    return false;
}

// Naive scan for ilst payload (fallback when structured parse misses it).
static std::vector<uint8_t> scan_ilst_payload(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        return {};
    }
    f.seekg(0, std::ios::end);
    std::streamoff len = f.tellg();
    if (len <= 0) {
        return {};
    }
    size_t sz = static_cast<size_t>(len);
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(sz);
    f.read(reinterpret_cast<char *>(data.data()), sz);
    for (size_t i = 0; i + 4 <= data.size(); ++i) {
        if (data[i] == 'i' && data[i + 1] == 'l' && data[i + 2] == 's' && data[i + 3] == 't') {
            if (i < 4) {
                continue;
            }
            uint32_t size =
                (data[i - 4] << 24) | (data[i - 3] << 16) | (data[i - 2] << 8) | (data[i - 1]);
            if (size < 8) {
                continue;
            }
            size_t payload_size = size - 8;
            if (i + 4 + payload_size <= data.size()) {
                return std::vector<uint8_t>(data.begin() + i + 4,
                                            data.begin() + i + 4 + payload_size);
            }
        }
    }
    return {};
}

// Parse mdhd to get timescale + duration.
static void parse_mdhd(std::istream &in, uint64_t size, uint32_t &timescale, uint64_t &duration) {
    uint8_t version = in.get();
    uint8_t flags[3];
    in.read((char *)flags, 3);

    if (version == 1) {
        read_u64(in);  // creation_time
        read_u64(in);  // modification_time
        timescale = read_u32(in);
        duration = read_u64(in);
    } else {
        read_u32(in);  // creation_time
        read_u32(in);  // modification_time
        timescale = read_u32(in);
        duration = read_u32(in);
    }
    // skip the rest of mdhd.
    uint64_t consumed = (version == 1 ? 1 + 3 + 8 + 8 + 4 + 8 : 1 + 3 + 4 + 4 + 4 + 4);
    if (size > consumed) {
        skip(in, size - consumed);
    }
}

// Extract ilst from meta payload (payload only, after size/type).
static void parse_meta_payload(std::istream &in, uint64_t size, ParsedMp4 &out) {
    const uint64_t start = (uint64_t)in.tellg();

    // meta full box header.
    uint8_t version = in.get();
    uint8_t flags[3];
    in.read((char *)flags, 3);
    (void)version;
    (void)flags;

    // Peek ahead to guess whether the next word is a valid atom size/type (ISO).
    const uint64_t payload_remain = size - 4;
    bool iso_style_first = true;
    if (payload_remain >= kAtomHeaderSize) {
        std::streampos after_flags = in.tellg();
        uint32_t next_size = read_u32(in);
        uint32_t next_type = read_u32(in);
        bool looks_like_atom = next_size >= kAtomHeaderSize && next_size <= payload_remain &&
                               is_printable_fourcc(next_type);
        iso_style_first = looks_like_atom;
        in.clear();
        in.seekg(after_flags);
    }

    auto parse_children = [&](bool consume_reserved) {
        in.clear();
        in.seekg(start + 4);  // after version+flags
        uint64_t remain = payload_remain;
        if (consume_reserved && remain >= kMetaReservedBytes) {
            read_u32(in);
            remain -= kMetaReservedBytes;
        }
        while (remain > kAtomHeaderSize) {
            auto child = read_atom_header(in);
            if (!in || child.size == 0 || child.size < kAtomHeaderSize || child.size > remain) {
                CH_LOG("debug", "meta child invalid size=" << child.size << " remain=" << remain);
                break;
            }
            uint64_t payload_size = child.size - kAtomHeaderSize;
            if (payload_size > remain - kAtomHeaderSize) {
                CH_LOG("debug", "meta child payload exceeds remain; breaking");
                break;
            }
            if (child.type == fourcc("ilst")) {
                out.ilst_payload = read_bytes(in, payload_size);
                CH_LOG("debug", "captured ilst payload, bytes=" << out.ilst_payload.size());
            } else {
                skip(in, payload_size);
            }
            remain -= child.size;
        }
    };

    // Try the likely style first, then fall back to the other.
    if (iso_style_first) {
        parse_children(false);
        if (out.ilst_payload.empty()) {
            parse_children(true);
        }
    } else {
        parse_children(true);
        if (out.ilst_payload.empty()) {
            parse_children(false);
        }
    }

    // Seek to end of meta box.
    uint64_t end = start + size;
    in.seekg(end);
}

// Parse hdlr to retrieve handler type and name. Expects payload size (box size minus 8).
static void parse_hdlr(std::istream &in, uint64_t payload_size, TrackParseResult &track) {
    if (payload_size < kHdlrMinPayload) {  // too small to contain required fields
        skip(in, payload_size);
        return;
    }

    uint8_t version = in.get();
    uint8_t flags[3];
    in.read((char *)flags, 3);
    (void)version;
    (void)flags;

    // pre_defined + handler_type.
    skip(in, 4);
    track.handler_type = read_u32(in);

    // reserved (12 bytes), then a Pascal-style or null-terminated UTF-8 name.
    uint64_t consumed = 1 + 3 + 4 + 4 + 12;
    std::string name;
    if (payload_size > consumed) {
        uint64_t name_len = payload_size - consumed;
        name.resize(static_cast<size_t>(name_len));
        in.read(name.data(), (std::streamsize)name_len);
        // Trim any trailing nulls.
        while (!name.empty() && name.back() == '\0') {
            name.pop_back();
        }
    }
    track.handler_name = std::move(name);
}

// Parse tkhd to retrieve track id and flags.
static void parse_tkhd(std::istream &in, uint64_t payload_size, TrackParseResult &track) {
    if (payload_size < 20) {
        skip(in, payload_size);
        return;
    }
    uint8_t version = in.get();
    uint8_t flags_bytes[3];
    in.read((char *)flags_bytes, 3);
    track.tkhd_flags = (flags_bytes[0] << 16) | (flags_bytes[1] << 8) | flags_bytes[2];
    // creation + modification time (version dependent); skip 8 or 16 bytes.
    uint64_t consumed = 1 + 3;
    if (version == 1) {
        skip(in, 16);
        consumed += 16;
    } else {
        skip(in, 8);
        consumed += 8;
    }
    track.track_id = read_u32(in);
    skip(in, 4);  // reserved
    consumed += 8;  // track_id + reserved
    // remainder not needed here.
    if (payload_size > consumed) {
        skip(in, payload_size - consumed);
    }
}

// Parse stbl children (stsd, stts, stsc, stsz, stco)
static void parse_stbl(std::istream &in, uint64_t size, TrackParseResult &track) {
    CH_LOG("debug", "parse_stbl size=" << size);
    uint64_t start = (uint64_t)in.tellg();
    uint64_t remain = size;

    while (remain >= 8) {
        auto info = read_atom_header(in);
        if (!in || info.size == 0 || info.size < 8) {
            break;
        }
        uint64_t payload_size = info.size - 8;

        switch (info.type) {
            case ('s' << 24 | 't' << 16 | 's' << 8 | 'd'):  // stsd
                if (track.stsd.empty()) {
                    track.stsd = read_bytes(in, payload_size);
                } else {
                    skip(in, payload_size);
                }
                break;
            case ('s' << 24 | 't' << 16 | 't' << 8 | 's'):  // stts
                if (track.stts.empty()) {
                    track.stts = read_bytes(in, payload_size);
                } else {
                    skip(in, payload_size);
                }
                break;
            case ('s' << 24 | 't' << 16 | 's' << 8 | 'c'):  // stsc
                if (track.stsc.empty()) {
                    track.stsc = read_bytes(in, payload_size);
                } else {
                    skip(in, payload_size);
                }
                break;
            case ('s' << 24 | 't' << 16 | 's' << 8 | 'z'):  // stsz
                if (track.stsz.empty()) {
                    track.stsz = read_bytes(in, payload_size);
                } else {
                    skip(in, payload_size);
                }
                break;
            case ('s' << 24 | 't' << 16 | 'c' << 8 | 'o'):  // stco
                if (track.stco.empty()) {
                    track.stco = read_bytes(in, payload_size);
                } else {
                    skip(in, payload_size);
                }
                break;
            default:
                skip(in, payload_size);
        }

        remain -= info.size;
    }

    uint64_t end = start + size;
    in.seekg(end);
    CH_LOG("debug", "stbl parsed sizes stsd=" << track.stsd.size()
                                               << " stts=" << track.stts.size()
                                               << " stsc=" << track.stsc.size()
                                               << " stsz=" << track.stsz.size()
                                               << " stco=" << track.stco.size());
}

// Parse mdia box of a track.
static bool parse_mdia(std::istream &in, uint64_t mpay, uint64_t mdia_end,
                       TrackParseResult &track, bool &force_fallback) {
    uint64_t mdia_remain = mpay;
    bool mdia_overflow = false;

    while (mdia_remain >= 8) {
        CH_LOG("debug", " mdia pos=" << (uint64_t)in.tellg() << " remain=" << mdia_remain);
        auto m = read_atom_header(in);
        if (!in || m.size == 0 || m.size < 8) {
            CH_LOG("debug", "mdia break: stream bad or size<8");
            break;
        }
        uint64_t mpay_child = m.size - 8;
        if (m.size > mdia_remain) {
            CH_LOG("debug", " mdia child type=" << fourcc_to_string(m.type) << " claims size="
                                                << m.size << " but remain=" << mdia_remain
                                                << " — clamping and bailing from mdia");
            mdia_overflow = true;
            in.seekg(mdia_end);
            mdia_remain = 0;
            break;
        }
        CH_LOG("debug", " mdia child type=" << fourcc_to_string(m.type) << " size=" << m.size
                                            << " offset=" << m.offset);

        // If a bogus child claims to run past mdia, clamp it but keep parsing so we can avoid a
        // full fallback where possible.
        if (m.offset + m.size > mdia_end) {
            uint64_t allowed = mdia_end - m.offset;
            if (allowed < 8) {
                CH_LOG("debug", " mdia child overflow too large; bail");
                mdia_overflow = true;
                in.seekg(mdia_end);
                mdia_remain = 0;
                break;
            }
            CH_LOG("debug", " mdia child overflow; clamping size " << m.size << " -> " << allowed);
            m.size = allowed;
            mpay_child = m.size - 8;
        }

        if (m.type == ('m' << 24 | 'd' << 16 | 'h' << 8 | 'd')) {
            parse_mdhd(in, mpay_child, track.timescale, track.duration);
            CH_LOG("debug", "  mdhd timescale=" << track.timescale
                                                << " duration=" << track.duration);
        } else if (m.type == ('h' << 24 | 'd' << 16 | 'l' << 8 | 'r')) {
            // Pass the payload size (box size minus header) so the handler name length is read
            // correctly.
            parse_hdlr(in, mpay_child, track);
            CH_LOG("debug", "  hdlr=" << std::hex << track.handler_type << std::dec
                                      << " name=" << track.handler_name);
        } else if (m.type == ('m' << 24 | 'i' << 16 | 'n' << 8 | 'f')) {
            // find stbl.
            uint64_t minf_end = (uint64_t)in.tellg() + mpay_child;
            uint64_t minf_rem = mpay_child;
            CH_LOG("debug", "  enter minf end=" << minf_end);

            while (minf_rem >= 8) {
                auto mi = read_atom_header(in);
                if (!in || mi.size < 8) {
                    CH_LOG("debug", "  minf break: stream bad or size<8");
                    break;
                }
                uint64_t mi_pay = mi.size - 8;
                CH_LOG("debug", "  minf child=" << std::hex << mi.type << std::dec
                                                 << " size=" << mi.size);

                if (mi.type == ('s' << 24 | 't' << 16 | 'b' << 8 | 'l')) {
                    parse_stbl(in, mi_pay, track);
                } else {
                    skip(in, mi_pay);
                }
                minf_rem -= mi.size;
            }
            in.seekg(minf_end);
        } else {
            // Try to salvage by scanning this mdia child payload for an embedded 'minf' (some
            // malformed files nest it oddly).
            if (mpay_child > 0 && mpay_child < 256 * 1024) {  // cap read size
                auto buf = read_bytes(in, mpay_child);
                const uint32_t minf_tag = ('m' << 24 | 'i' << 16 | 'n' << 8 | 'f');
                for (size_t off = 0; off + 4 <= buf.size(); ++off) {
                    uint32_t maybe =
                        (buf[off] << 24) | (buf[off + 1] << 16) | (buf[off + 2] << 8) |
                        buf[off + 3];
                    if (maybe == minf_tag) {
                        uint64_t abs_pos = m.offset + 8 + off;
                        CH_LOG("debug", "  found nested minf @" << abs_pos << " inside "
                                                                << fourcc_to_string(m.type));
                        in.seekg((std::streamoff)abs_pos);
                        uint64_t minf_remaining = mdia_end - abs_pos;
                        parse_stbl(in, minf_remaining, track);
                        in.seekg(mdia_end);
                        mdia_remain = 0;
                        goto mdia_done;
                    }
                }
            } else {
                skip(in, mpay_child);
            }
        }
        if (m.size > mdia_remain) {
            break;
        }
        // Ensure we land exactly at the end of this child before reading the next atom, even if the
        // parser above consumed slightly less than declared.
        in.seekg((std::streamoff)(m.offset + m.size));
        mdia_remain -= m.size;
    }
mdia_done:
    in.seekg(mdia_end);
    if (mdia_overflow) {
        force_fallback = true;
        return false;
    }
    return true;
}

// Parse trak box and return a TrackParseResult (audio/video/text).
static std::optional<TrackParseResult> parse_trak(std::istream &in, uint64_t c_payload,
                                                  uint64_t file_size, bool &force_fallback) {
    (void)file_size;
    uint64_t trak_end = (uint64_t)in.tellg() + c_payload;
    uint64_t trak_remain = c_payload;
    TrackParseResult track;
    CH_LOG("debug", "trak start end=" << trak_end);

    while (trak_remain >= 8) {
        auto tchild = read_atom_header(in);
        if (!in || tchild.size == 0 || tchild.size < 8) {
            break;
        }
        uint64_t tpay = tchild.size - 8;
        CH_LOG("debug", " trak child=" << fourcc_to_string(tchild.type) << " size=" << tchild.size);

        if (tchild.type == ('t' << 24 | 'k' << 16 | 'h' << 8 | 'd')) {
            parse_tkhd(in, tpay, track);
        } else if (tchild.type == ('m' << 24 | 'd' << 16 | 'i' << 8 | 'a')) {
            CH_LOG("debug", "trak: entering mdia");
            uint64_t mdia_end = (uint64_t)in.tellg() + tpay;
            parse_mdia(in, tpay, mdia_end, track, force_fallback);
            if (force_fallback) {
                break;
            }
        } else {
            skip(in, tpay);
        }
        trak_remain -= tchild.size;
        if (force_fallback) {
            break;
        }
    }

    if (force_fallback) {
        return std::nullopt;
    }

    if (track.stsz.size() >= 12) {
        track.sample_count = (track.stsz[8] << 24) | (track.stsz[9] << 16) |
                             (track.stsz[10] << 8) | track.stsz[11];
    }

    return track;
}

// Parse moov atom.
static void parse_moov(std::istream &in, const Mp4AtomInfo &atom, uint64_t file_size,
                       ParsedMp4 &out, uint32_t &best_audio_samples, bool &force_fallback) {
    uint64_t end = atom.offset + atom.size;
    if (end > file_size) {
        CH_LOG("error", "parse_mp4: moov exceeds file size end=" << end << " file=" << file_size);
        in.seekg(atom.offset + atom.size, std::ios::beg);
        return;
    }

    CH_LOG("debug", "enter moov @0x" << std::hex << atom.offset << std::dec << " end=" << end);
    while (((uint64_t)in.tellg()) + 8 <= end) {
        auto child = read_atom_header(in);
        if (!in || child.size == 0 || child.size < 8) {
            break;
        }

        if (child.offset + child.size > end) {
                CH_LOG("error", "parse_mp4: child overflow type=" << fourcc_to_string(child.type)
                                                                 << " size=" << child.size
                                                                 << " end=" << end);
            child.size = end - child.offset;
            if (child.size < 8) {
                break;
            }
        }

        uint64_t c_payload = child.size - 8;
        CH_LOG("debug", "moov child=" << fourcc_to_string(child.type) << " size=" << child.size
                                      << " offset=0x" << std::hex << child.offset << std::dec);

        switch (child.type) {
            case ('u' << 24 | 'd' << 16 | 't' << 8 | 'a'): {
                uint64_t udta_end = (uint64_t)in.tellg() + c_payload;
                while (((uint64_t)in.tellg()) + 8 <= udta_end) {
                    auto u = read_atom_header(in);
                    if (!in || u.size < 8) {
                        break;
                    }
                    uint64_t upay = u.size - 8;

                    if (u.type == ('m' << 24 | 'e' << 16 | 't' << 8 | 'a')) {
                        CH_LOG("debug", "found meta inside udta");
                        auto meta_buf = read_bytes(in, upay);
                        if (out.meta_payload.empty()) {
                            out.meta_payload = meta_buf;
                        }
                        std::string meta_str(meta_buf.begin(), meta_buf.end());
                        std::istringstream meta_stream(meta_str);
                        parse_meta_payload(meta_stream, upay, out);
                    } else {
                        skip(in, upay);
                    }
                }
                break;
            }
            case ('m' << 24 | 'e' << 16 | 't' << 8 | 'a'): {
                CH_LOG("debug", "found meta under moov");
                auto meta_buf = read_bytes(in, c_payload);
                if (out.meta_payload.empty()) {
                    out.meta_payload = meta_buf;
                }
                std::string meta_str(meta_buf.begin(), meta_buf.end());
                std::istringstream meta_stream(meta_str);
                parse_meta_payload(meta_stream, c_payload, out);
                break;
            }
            case ('t' << 24 | 'r' << 16 | 'a' << 8 | 'k'): {
                auto track_opt = parse_trak(in, c_payload, file_size, force_fallback);
                if (!track_opt) {
                    break;
                }
                auto &track = *track_opt;
                out.tracks.push_back(track);
                if (track.handler_type == fourcc("soun")) {
                    if (!track.stsz.empty() &&
                        (best_audio_samples == 0 || track.sample_count >= best_audio_samples)) {
                        best_audio_samples = track.sample_count;
                        out.audio_timescale = track.timescale;
                        out.audio_duration = track.duration;
                        out.stsd = track.stsd;
                        out.stts = track.stts;
                        out.stsc = track.stsc;
                        out.stsz = track.stsz;
                        out.stco = track.stco;
                    }
                }
                break;
            }
            default:
                skip(in, c_payload);
                break;
        }
    }
    in.seekg(end);
}

//
// Main MP4 parsing.
//
std::optional<ParsedMp4> parse_mp4(const std::string &path) {
    const auto t_start = std::chrono::steady_clock::now();
    ParsedMp4 out;
    uint32_t best_audio_samples = 0;
    bool force_fallback = false;

    CH_LOG("debug", "parse_mp4 enter path=" << path);
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        CH_LOG("error", "parse_mp4: cannot open " << path << " errno=" << errno);
        return std::nullopt;
    }

    in.seekg(0, std::ios::end);
    const uint64_t file_size = static_cast<uint64_t>(in.tellg());
    in.seekg(0, std::ios::beg);
    CH_LOG("debug", "parse_mp4: size=" << file_size << " path=" << path);

    while (in.peek() != EOF) {
        if (force_fallback) {
            break;
        }
        Mp4AtomInfo atom = read_atom_header(in);
        if (atom.size == 0) {
            CH_LOG("warn", "parse_mp4: atom with zero/invalid size encountered, bailing");
            break;
        }
        if (atom.size < 8 || atom.offset + atom.size > file_size) {
            CH_LOG("error", "parse_mp4: bad atom header size=" << atom.size
                                                               << " offset=" << atom.offset
                                                               << " file=" << file_size);
            break;
        }
        uint64_t payload_size = atom.size - 8;

        switch (atom.type) {
            case ('m' << 24 | 'd' << 16 | 'a' << 8 | 't'): {  // mdat
                // Skip mdat payload; we only need sample tables for reuse.
                skip(in, payload_size);
                break;
            }
            case ('m' << 24 | 'o' << 16 | 'o' << 8 | 'v'): {  // moov
                parse_moov(in, atom, file_size, out, best_audio_samples, force_fallback);
                break;
            }
            default:
                skip(in, payload_size);
        }
    }

    const auto t_struct_done = std::chrono::steady_clock::now();

    // Fallback: flat scan for sample-table atoms if they were not captured.
    if (out.stsz.empty() || out.stco.empty() || out.stsc.empty() || out.stsd.empty()) {
        CH_LOG("debug", "fallback flat scan for stbl atoms");
        out.used_fallback_stbl = true;
        // load whole file into memory and search for atoms by signature.
        in.clear();
        in.seekg(0, std::ios::end);
        std::streamoff len = in.tellg();
        if (len <= 0) {
            return out;
        }
        in.seekg(0, std::ios::beg);
        std::vector<uint8_t> buf(static_cast<size_t>(len));
        in.read(reinterpret_cast<char *>(buf.data()), len);

        grab_atom_from_buffer(buf, "stsd", out.stsd);
        grab_atom_from_buffer(buf, "stts", out.stts);
        grab_atom_from_buffer(buf, "stsc", out.stsc);
        grab_atom_from_buffer(buf, "stsz", out.stsz);
        grab_atom_from_buffer(buf, "stco", out.stco);
        if (out.ilst_payload.empty()) {
            grab_atom_from_buffer(buf, "ilst", out.ilst_payload);
        }
    }

    // Fallback scan for ilst if still missing.
    if (out.ilst_payload.empty()) {
        auto ilst = scan_ilst_payload(path);
        if (!ilst.empty()) {
            out.ilst_payload = std::move(ilst);
            CH_LOG("debug", "ilst found via naive scan, bytes=" << out.ilst_payload.size());
        }
    }
    const auto t_fallback_done = std::chrono::steady_clock::now();

    if (out.stco.empty() || out.stsc.empty() || out.stsz.empty() || out.stsd.empty()) {
        CH_LOG("error", "parse_mp4: missing stbl atoms stco/stsc/stsz/stsd");
    }
    const auto t_done = std::chrono::steady_clock::now();
    const auto ms_struct =
        std::chrono::duration_cast<std::chrono::milliseconds>(t_struct_done - t_start).count();
    const auto ms_fallback =
        std::chrono::duration_cast<std::chrono::milliseconds>(t_fallback_done - t_struct_done)
            .count();
    const auto ms_total =
        std::chrono::duration_cast<std::chrono::milliseconds>(t_done - t_start).count();

    CH_LOG("debug", "parse_mp4 done stco=" << out.stco.size() << " stsc=" << out.stsc.size()
                                           << " stsz=" << out.stsz.size()
                                           << " stsd=" << out.stsd.size()
                                           << " ilst=" << out.ilst_payload.size()
                                           << " timescale=" << out.audio_timescale
                                           << " duration=" << out.audio_duration
                                           << " fallback_stbl=" << out.used_fallback_stbl
                                           << " timings_ms struct=" << ms_struct
                                           << " fallback=" << ms_fallback
                                           << " total=" << ms_total);
    return out;
}

#ifdef CHAPTERFORGE_TESTING
std::optional<TrackParseResult> parse_trak_for_test(std::istream &in, uint64_t payload_size,
                                                    uint64_t file_size, bool &force_fallback) {
    return parse_trak(in, payload_size, file_size, force_fallback);
}

void parse_moov_for_test(std::istream &in, const Mp4AtomInfo &atom, uint64_t file_size,
                         ParsedMp4 &out, uint32_t &best_audio_samples, bool &force_fallback) {
    parse_moov(in, atom, file_size, out, best_audio_samples, force_fallback);
}
#endif
