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
#include <iomanip>
#include <stdexcept>
#include <sstream>

#include "logging.hpp"
#include "mp4_atoms.hpp"

namespace {

constexpr uint64_t kAtomHeaderSize = 8;
constexpr uint64_t kFullBoxBaseHeader = 12;  // size+type+version/flags
constexpr uint64_t kMetaReservedBytes = 4;
constexpr uint64_t kHdlrMinPayload = 20;

static bool is_printable_fourcc(uint32_t type) {
    for (int i = 0; i < 4; ++i) {
        uint8_t c = (type >> (24 - 8 * i)) & 0xFF;
        if (c < 0x20 || c > 0x7E) {
            return false;
        }
    }
    return true;
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
    return info;
}

// Utility: read an atom payload into a byte buffer.
static std::vector<uint8_t> read_bytes(std::istream &in, uint64_t size) {
    std::vector<uint8_t> buf(size);
    in.read(reinterpret_cast<char *>(buf.data()), size);
    return buf;
}

// Render a short hex+ASCII preview from a given offset without changing the stream position.
static std::string hex_preview(std::istream &in, uint64_t offset, uint64_t max_len) {
    std::string out;
    std::streampos cur = in.tellg();
    in.seekg(offset);
    const uint64_t len = std::min<uint64_t>(max_len, 64);
    std::vector<uint8_t> buf(len);
    in.read(reinterpret_cast<char *>(buf.data()), len);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < buf.size(); ++i) {
        oss << std::setw(2) << static_cast<unsigned>(buf[i]) << " ";
    }
    oss << "| ";
    for (uint8_t b : buf) {
        oss << (std::isprint(b) ? static_cast<char>(b) : '.');
    }
    out = oss.str();
    in.clear();
    in.seekg(cur);
    return out;
}

// Naive scan for ilst payload (fallback when structured parse misses it)
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

// Extract ilst from meta.
static void parse_meta(std::istream &in, uint64_t size, ParsedMp4 &out) {
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
            if (!in || child.size < kAtomHeaderSize || child.size > remain) {
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

// Parse hdlr to retrieve handler type. Expects payload size (box size minus 8).
static uint32_t parse_hdlr(std::istream &in, uint64_t payload_size) {
    if (payload_size < kHdlrMinPayload) {  // too small to contain required fields
        skip(in, payload_size);
        return 0;
    }

    uint8_t version = in.get();
    uint8_t flags[3];
    in.read((char *)flags, 3);
    (void)version;
    (void)flags;

    // pre_defined + handler_type.
    skip(in, 4);
    uint32_t handler_type = read_u32(in);

    // skip reserved + name.
    uint64_t consumed = 1 + 3 + 4 + 4 + 12;
    if (payload_size > consumed) {
        skip(in, payload_size - consumed);
    }
    return handler_type;
}

// Parse stbl children (stsd, stts, stsc, stsz, stco)
static void parse_stbl(std::istream &in, uint64_t size, std::vector<uint8_t> &stsd,
                       std::vector<uint8_t> &stts, std::vector<uint8_t> &stsc,
                       std::vector<uint8_t> &stsz, std::vector<uint8_t> &stco) {
    CH_LOG("debug", "parse_stbl size=" << size);
    uint64_t start = (uint64_t)in.tellg();
    uint64_t remain = size;

    while (remain >= 8) {
        auto info = read_atom_header(in);
        if (!in || info.size < 8) {
            break;
        }
        uint64_t payload_size = info.size - 8;

        switch (info.type) {
            case ('s' << 24 | 't' << 16 | 's' << 8 | 'd'):  // stsd
                if (stsd.empty()) {
                    stsd = read_bytes(in, payload_size);
                } else {
                    skip(in, payload_size);
                }
                break;
            case ('s' << 24 | 't' << 16 | 't' << 8 | 's'):  // stts
                if (stts.empty()) {
                    stts = read_bytes(in, payload_size);
                } else {
                    skip(in, payload_size);
                }
                break;
            case ('s' << 24 | 't' << 16 | 's' << 8 | 'c'):  // stsc
                if (stsc.empty()) {
                    stsc = read_bytes(in, payload_size);
                } else {
                    skip(in, payload_size);
                }
                break;
            case ('s' << 24 | 't' << 16 | 's' << 8 | 'z'):  // stsz
                if (stsz.empty()) {
                    stsz = read_bytes(in, payload_size);
                } else {
                    skip(in, payload_size);
                }
                break;
            case ('s' << 24 | 't' << 16 | 'c' << 8 | 'o'):  // stco
                if (stco.empty()) {
                    stco = read_bytes(in, payload_size);
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
    CH_LOG("debug", "stbl parsed sizes stsd=" << stsd.size() << " stts=" << stts.size()
                                               << " stsc=" << stsc.size() << " stsz=" << stsz.size()
                                               << " stco=" << stco.size());
}

//
// Main MP4 parsing.
//
std::optional<ParsedMp4> parse_mp4(const std::string &path) {
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
        if (atom.size < 8 || atom.offset + atom.size > file_size) {
            CH_LOG("error", "parse_mp4: bad atom header size=" << atom.size
                                                               << " offset=" << atom.offset
                                                               << " file=" << file_size);
            break;
        }
        uint64_t payload_size = atom.size - 8;

    switch (atom.type) {
        case ('m' << 24 | 'd' << 16 | 'a' << 8 | 't'): {  // mdat
            auto data = read_bytes(in, payload_size);
                if (data.size() > out.mdat_data.size()) {
                    out.mdat_data = std::move(data);
                    out.mdat_size = payload_size;
                }
                break;
            }

            case ('m' << 24 | 'o' << 16 | 'o' << 8 | 'v'): {  // moov
                uint64_t end = atom.offset + atom.size;
                if (end > file_size) {
                    CH_LOG("error", "parse_mp4: moov exceeds file size end=" << end
                                                                            << " file=" << file_size);
                    in.seekg(atom.offset + atom.size, std::ios::beg);
                    break;
                }

                CH_LOG("debug", "enter moov @0x" << std::hex << atom.offset << std::dec
                                                << " end=" << end);
                while (((uint64_t)in.tellg()) + 8 <= end) {
                    auto child = read_atom_header(in);
                    if (!in || child.size < 8) {
                        break;
                    }

                    // Clamp malformed child atoms that spill past moov. Some files report sizes
                    // that overrun the parent atom; we log and clamp instead of bailing out early.
                    if (child.offset + child.size > end) {
                        CH_LOG("error", "parse_mp4: child overflow type=" << std::hex << child.type
                                                                          << std::dec
                                                                          << " size=" << child.size
                                                                          << " end=" << end);
                        child.size = end - child.offset;
                        if (child.size < 8) {
                            break;
                        }
                    }

                    uint64_t c_payload = child.size - 8;
                    CH_LOG("debug", "moov child=" << std::hex << child.type << std::dec
                                                   << " size=" << child.size
                                                   << " offset=0x" << std::hex << child.offset
                                                   << std::dec);

                    switch (child.type) {
                        case ('u' << 24 | 'd' << 16 | 't' << 8 | 'a'):
                            // find meta inside udta.
                            {
                                uint64_t udta_end = (uint64_t)in.tellg() + c_payload;
                                while (((uint64_t)in.tellg()) + 8 <= udta_end) {
                                    auto u = read_atom_header(in);
                                    if (!in || u.size < 8) {
                                        break;
                                    }
                                    uint64_t upay = u.size - 8;

                                    if (u.type == ('m' << 24 | 'e' << 16 | 't' << 8 | 'a')) {
                                        CH_LOG("debug", "found meta inside udta");
                                        parse_meta(in, upay, out);
                                    } else {
                                        skip(in, upay);
                                    }
                                }
                                break;
                            }

                        case ('m' << 24 | 'e' << 16 | 't' << 8 | 'a'): {
                            // Some files place meta directly under moov (not inside.
                            // udta)
                            CH_LOG("debug", "found meta under moov");
                            parse_meta(in, c_payload, out);
                            break;
                        }

                        case ('t' << 24 | 'r' << 16 | 'a' << 8 | 'k'): {
                            uint64_t trak_end = (uint64_t)in.tellg() + c_payload;
                            uint64_t trak_remain = c_payload;
                            uint32_t handler_type = 0;
                            uint32_t mdhd_timescale = 0;
                            uint64_t mdhd_duration = 0;
                            std::vector<uint8_t> stsd, stts, stsc, stsz, stco;
                    CH_LOG("debug", "trak start end=" << trak_end);

                            while (trak_remain >= 8) {
                                auto tchild = read_atom_header(in);
                                if (!in || tchild.size < 8) {
                                    break;
                                }
                                uint64_t tpay = tchild.size - 8;
                                CH_LOG("debug", " trak child=" << std::hex << tchild.type
                                                                << std::dec << " size=" << tchild.size);

                                if (tchild.type == ('m' << 24 | 'd' << 16 | 'i' << 8 | 'a')) {
                                    CH_LOG("debug", "trak: entering mdia");
                                    // parse mdia.
                                    uint64_t mdia_end = (uint64_t)in.tellg() + tpay;
                                    uint64_t mdia_remain = tpay;
                                    bool mdia_overflow = false;

                                    while (mdia_remain >= 8) {
                                        CH_LOG("debug", " mdia pos=" << (uint64_t)in.tellg()
                                                                      << " remain=" << mdia_remain);
                                        auto m = read_atom_header(in);
                                    if (!in || m.size < 8) {
                                        CH_LOG("debug", "mdia break: stream bad or size<8");
                                        break;
                                    }
                                    uint64_t mpay = m.size - 8;
                                    if (m.size > mdia_remain) {
                                        CH_LOG("debug",
                                               " mdia child type=" << std::hex << m.type
                                                                   << std::dec
                                                                   << " claims size=" << m.size
                                                                   << " but remain=" << mdia_remain
                                                                   << " — clamping and bailing "
                                                                      "from mdia");
                                        mdia_overflow = true;
                                        in.seekg(mdia_end);
                                        mdia_remain = 0;
                                        break;
                                    }
                                    CH_LOG("debug",
                                           " mdia child type=" << std::hex << m.type << std::dec
                                                               << " size=" << m.size
                                                               << " offset=" << m.offset);

                                    // If a bogus child claims to run past mdia, clamp it but keep
                                    // parsing so we can avoid a full fallback where possible.
                                    if (m.offset + m.size > mdia_end) {
                                        uint64_t allowed = mdia_end - m.offset;
                                        if (allowed < 8) {
                                            CH_LOG("debug",
                                                   " mdia child overflow too large; bail");
                                            mdia_overflow = true;
                                            in.seekg(mdia_end);
                                            mdia_remain = 0;
                                            break;
                                        }
                                        CH_LOG("debug", " mdia child overflow; clamping size "
                                                             << m.size << " -> " << allowed);
                                        m.size = allowed;
                                        mpay = m.size - 8;
                                    }

                                    if (m.type == ('m' << 24 | 'd' << 16 | 'h' << 8 | 'd')) {
                                        parse_mdhd(in, mpay, mdhd_timescale, mdhd_duration);
                                        CH_LOG("debug", "  mdhd timescale=" << mdhd_timescale
                                                                            << " duration="
                                                                            << mdhd_duration);
                                        } else if (m.type ==
                                                   ('h' << 24 | 'd' << 16 | 'l' << 8 | 'r')) {
                                            handler_type = parse_hdlr(in, m.size);
                                            CH_LOG("debug",
                                                   "  hdlr=" << std::hex << handler_type
                                                             << std::dec);
                                        } else if (m.type ==
                                                   ('m' << 24 | 'i' << 16 | 'n' << 8 | 'f')) {
                                            // find stbl.
                                            uint64_t minf_end = (uint64_t)in.tellg() + mpay;
                                            uint64_t minf_rem = mpay;
                                            CH_LOG("debug", "  enter minf end=" << minf_end);

                                            while (minf_rem >= 8) {
                                                auto mi = read_atom_header(in);
                                                if (!in || mi.size < 8) {
                                                    CH_LOG("debug",
                                                           "  minf break: stream bad or size<8");
                                                    break;
                                                }
                                                uint64_t mi_pay = mi.size - 8;
                                                CH_LOG("debug", "  minf child=" << std::hex
                                                                                 << mi.type
                                                                                 << std::dec
                                                                                 << " size="
                                                                                 << mi.size);

                                                if (mi.type ==
                                                    ('s' << 24 | 't' << 16 | 'b' << 8 | 'l')) {
                                                    parse_stbl(in, mi_pay, stsd, stts, stsc, stsz,
                                                               stco);
                                                } else {
                                                    skip(in, mi_pay);
                                                }
                                                minf_rem -= mi.size;
                                            }
                                            in.seekg(minf_end);
                                        } else {
                                            // Try to salvage by scanning this mdia child payload for
                                            // an embedded 'minf' (some malformed files nest it oddly).
                                            if (mpay > 0 && mpay < 256 * 1024) {  // cap read size
                                                auto buf = read_bytes(in, mpay);
                                                const uint32_t minf_tag = ('m' << 24 | 'i' << 16 |
                                                                           'n' << 8 | 'f');
                                                for (size_t off = 0; off + 4 <= buf.size(); ++off) {
                                                    uint32_t maybe =
                                                        (buf[off] << 24) | (buf[off + 1] << 16) |
                                                        (buf[off + 2] << 8) | buf[off + 3];
                                                    if (maybe == minf_tag) {
                                                        uint64_t abs_pos = m.offset + 8 + off;
                                                        CH_LOG("debug", "  found nested minf @"
                                                                             << abs_pos << " inside "
                                                                             << std::hex << m.type
                                                                             << std::dec);
                                                        in.seekg((std::streamoff)abs_pos);
                                                        // Re-run minf parse from here.
                                                        uint64_t minf_remaining = mdia_end - abs_pos;
                                                        parse_stbl(in, minf_remaining, stsd, stts,
                                                                   stsc, stsz, stco);
                                                        // Position to end of mdia.
                                                        in.seekg(mdia_end);
                                                        mdia_remain = 0;
                                                        goto mdia_done;
                                                    }
                                                }
                                            } else {
                                                skip(in, mpay);
                                            }
                                        }
                                        if (m.size > mdia_remain) {
                                            break;
                                        }
                                        // Ensure we land exactly at the end of this child before
                                        // reading the next atom, even if the parser above consumed
                                        // slightly less than declared.
                                        in.seekg((std::streamoff)(m.offset + m.size));
                                        mdia_remain -= m.size;
                                    }
mdia_done:
                                    in.seekg(mdia_end);
                                    if (mdia_overflow) {
                                        // Structured mdia failed; trigger fallback scan path.
                                        force_fallback = true;
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

                            if (handler_type == fourcc("soun")) {
                                // prefer the audio track with the largest sample_count.
                                uint32_t sample_count = 0;
                                if (stsz.size() >= 12) {
                                    // stsz payload: version/flags(4), sample_size(4), sample_count(4).
                                    sample_count = (stsz[8] << 24) | (stsz[9] << 16) |
                                                   (stsz[10] << 8) | stsz[11];
                                }
                                // If we have no audio yet, keep the first parsed audio even if the
                                // sample_count field is zero (some sources omit it).
                                if (!stsz.empty() &&
                                    (best_audio_samples == 0 || sample_count >= best_audio_samples)) {
                                    best_audio_samples = sample_count;
                                    out.audio_timescale = mdhd_timescale;
                                    out.audio_duration = mdhd_duration;
                                    out.stsd = std::move(stsd);
                                    out.stts = std::move(stts);
                                    out.stsc = std::move(stsc);
                                    out.stsz = std::move(stsz);
                                    out.stco = std::move(stco);
                                }
                            }

                            in.seekg(trak_end);
                            break;
                        }

                        default:
                            skip(in, c_payload);
                    }

                    uint64_t child_end = child.offset + child.size;
                    in.seekg(child_end);
                    if (force_fallback) {
                        break;
                    }
                }
                in.seekg(end);
                break;
            }

            default:
                skip(in, payload_size);
        }
    }

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

        auto grab_atom = [&](const char *fourcc, std::vector<uint8_t> &dst) {
            if (!dst.empty()) {
                return;
            }
            for (size_t i = 0; i + 8 <= buf.size(); ++i) {
                if (buf[i + 4] == static_cast<uint8_t>(fourcc[0]) &&
                    buf[i + 5] == static_cast<uint8_t>(fourcc[1]) &&
                    buf[i + 6] == static_cast<uint8_t>(fourcc[2]) &&
                    buf[i + 7] == static_cast<uint8_t>(fourcc[3])) {
                    uint32_t sz =
                        (buf[i] << 24) | (buf[i + 1] << 16) | (buf[i + 2] << 8) | buf[i + 3];
                    uint64_t end = static_cast<uint64_t>(i) + static_cast<uint64_t>(sz);
                    if (sz >= 8 && end <= buf.size() && end >= i + 8) {
                        dst.assign(buf.begin() + i + 8, buf.begin() + end);
                        CH_LOG("debug",
                               "grabbed " << fourcc << " via raw scan, bytes=" << dst.size());
                        break;
                    }
                }
            }
        };

        grab_atom("stsd", out.stsd);
        grab_atom("stts", out.stts);
        grab_atom("stsc", out.stsc);
        grab_atom("stsz", out.stsz);
        grab_atom("stco", out.stco);
        if (out.ilst_payload.empty()) {
            grab_atom("ilst", out.ilst_payload);
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
    if (out.stco.empty() || out.stsc.empty() || out.stsz.empty() || out.stsd.empty()) {
        CH_LOG("error", "parse_mp4: missing stbl atoms stco/stsc/stsz/stsd");
    }
    CH_LOG("debug", "parse_mp4 done stco=" << out.stco.size() << " stsc=" << out.stsc.size()
                                            << " stsz=" << out.stsz.size()
                                            << " stsd=" << out.stsd.size()
                                            << " ilst=" << out.ilst_payload.size()
                                            << " timescale=" << out.audio_timescale
                                            << " duration=" << out.audio_duration
                                            << " fallback_stbl=" << out.used_fallback_stbl);
    return out;
}
