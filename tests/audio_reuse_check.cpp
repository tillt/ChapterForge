// Compares the audio media payload extracted via stsz/stco/stsc between.
// input and output MP4/M4A files.
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "parser.hpp"

static std::optional<std::vector<uint32_t>> parse_stsz_sizes(
    const std::vector<uint8_t> &stsz_payload) {
    if (stsz_payload.size() < 12) {
        return std::nullopt;
    }
    const uint8_t *p = stsz_payload.data();
    uint32_t sample_size = (p[4] << 24) | (p[5] << 16) | (p[6] << 8) | p[7];
    uint32_t sample_count = (p[8] << 24) | (p[9] << 16) | (p[10] << 8) | p[11];
    std::vector<uint32_t> sizes;
    if (sample_size != 0) {
        sizes.assign(sample_count, sample_size);
        return sizes;
    }
    if (stsz_payload.size() < 12 + sample_count * 4) {
        return std::nullopt;
    }
    sizes.reserve(sample_count);
    size_t pos = 12;
    for (uint32_t i = 0; i < sample_count; ++i) {
        uint32_t s = (p[pos] << 24) | (p[pos + 1] << 16) | (p[pos + 2] << 8) | p[pos + 3];
        sizes.push_back(s);
        pos += 4;
    }
    return sizes;
}

static std::vector<uint32_t> derive_chunk_plan(const std::vector<uint8_t> &stsc_payload,
                                               uint32_t sample_count) {
    std::vector<uint32_t> plan;
    if (stsc_payload.size() < 16) {
        return plan;
    }
    uint32_t entry_count = (stsc_payload[4] << 24) | (stsc_payload[5] << 16) |
                           (stsc_payload[6] << 8) | stsc_payload[7];
    size_t pos = 8;
    uint32_t consumed = 0;
    for (uint32_t i = 0; i < entry_count; ++i) {
        if (pos + 12 > stsc_payload.size()) {
            break;
        }
        uint32_t first_chunk = (stsc_payload[pos] << 24) | (stsc_payload[pos + 1] << 16) |
                               (stsc_payload[pos + 2] << 8) | stsc_payload[pos + 3];
        uint32_t samples_per_chunk = (stsc_payload[pos + 4] << 24) | (stsc_payload[pos + 5] << 16) |
                                     (stsc_payload[pos + 6] << 8) | stsc_payload[pos + 7];
        uint32_t next_first = 0;
        if (i + 1 < entry_count && pos + 12 <= stsc_payload.size() - 4) {
            next_first = (stsc_payload[pos + 12] << 24) | (stsc_payload[pos + 13] << 16) |
                         (stsc_payload[pos + 14] << 8) | (stsc_payload[pos + 15]);
        }
        uint32_t chunk_count = (next_first > 0) ? (next_first - first_chunk) : 0;
        if (chunk_count == 0) {
            while (consumed < sample_count) {
                plan.push_back(samples_per_chunk);
                consumed += samples_per_chunk;
            }
        } else {
            for (uint32_t c = 0; c < chunk_count && consumed < sample_count; ++c) {
                plan.push_back(samples_per_chunk);
                consumed += samples_per_chunk;
            }
        }
        pos += 12;
    }
    return plan;
}

static std::optional<std::vector<uint8_t>> extract_audio_bytes(const std::string &path) {
    auto parsed_opt = parse_mp4(path);
    if (!parsed_opt) {
        std::cerr << "[reuse] parse failed for " << path << "\n";
        return std::nullopt;
    }
    const ParsedMp4 &parsed = *parsed_opt;
    auto sizes_opt = parse_stsz_sizes(parsed.stsz);
    if (!sizes_opt || parsed.stco.empty() || parsed.stsc.empty()) {
        return std::nullopt;
    }
    const auto &sizes = *sizes_opt;

    std::vector<uint32_t> chunk_plan =
        derive_chunk_plan(parsed.stsc, static_cast<uint32_t>(sizes.size()));
    if (chunk_plan.empty()) {
        return std::nullopt;
    }

    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        return std::nullopt;
    }

    std::cerr << "[reuse] file=" << path << " samples=" << sizes.size()
              << " stco_bytes=" << parsed.stco.size() << " stsc_bytes=" << parsed.stsc.size()
              << " chunk_plan=" << chunk_plan.size() << "\n";

    std::vector<uint8_t> out;
    out.reserve(parsed.mdat_size ? parsed.mdat_size : sizes.size() * 200);

    const uint8_t *pco = parsed.stco.data();
    uint32_t stco_count = (pco[4] << 24) | (pco[5] << 16) | (pco[6] << 8) | pco[7];
    size_t stco_pos = 8;
    size_t sample_idx = 0;

    if (stco_count > 0) {
        std::cerr << "[reuse] stco entries=" << stco_count << " first offsets: ";
        for (uint32_t j = 0; j < std::min<uint32_t>(5, stco_count); ++j) {
            size_t pos = 8 + j * 4;
            uint32_t off =
                (pco[pos] << 24) | (pco[pos + 1] << 16) | (pco[pos + 2] << 8) | pco[pos + 3];
            std::cerr << off << (j + 1 < std::min<uint32_t>(5, stco_count) ? ", " : "");
        }
        std::cerr << "\n";
    }

    for (uint32_t chunk_idx = 0; chunk_idx < stco_count && chunk_idx < chunk_plan.size();
         ++chunk_idx) {
        if (stco_pos + 4 > parsed.stco.size()) {
            break;
        }
        uint32_t chunk_offset = (pco[stco_pos] << 24) | (pco[stco_pos + 1] << 16) |
                                (pco[stco_pos + 2] << 8) | pco[stco_pos + 3];
        stco_pos += 4;
        uint32_t samples_in_chunk = chunk_plan[chunk_idx];
        uint64_t chunk_size = 0;
        for (uint32_t i = 0; i < samples_in_chunk && sample_idx + i < sizes.size(); ++i) {
            chunk_size += sizes[sample_idx + i];
        }
        if (chunk_size == 0) {
            break;
        }
        std::vector<uint8_t> chunk(chunk_size);
        f.seekg(static_cast<std::streamoff>(chunk_offset), std::ios::beg);
        f.read(reinterpret_cast<char *>(chunk.data()), static_cast<std::streamsize>(chunk_size));
        if (f.gcount() != static_cast<std::streamsize>(chunk_size)) {
            std::cerr << "[reuse] short read at chunk " << chunk_idx << " offset=" << chunk_offset
                      << " expected=" << chunk_size << " got=" << f.gcount() << "\n";
            break;
        }
        out.insert(out.end(), chunk.begin(), chunk.end());
        sample_idx += samples_in_chunk;
    }

    if (sample_idx != sizes.size()) {
        std::cerr << "[reuse] sample_idx=" << sample_idx << " expected=" << sizes.size() << "\n";
        return std::nullopt;
    }
    return out;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "usage: audio_reuse_check <input.m4a> <output.m4a>\n";
        return 2;
    }
    auto in_bytes = extract_audio_bytes(argv[1]);
    auto out_bytes = extract_audio_bytes(argv[2]);
    if (!in_bytes || !out_bytes) {
        std::cerr << "failed to extract audio bytes from one of the files\n";
        return 3;
    }
    if (*in_bytes == *out_bytes) {
        std::cout << "OK: output audio payload matches input (" << in_bytes->size() << " bytes)\n";
        return 0;
    }
    size_t diff = 0;
    for (; diff < in_bytes->size() && diff < out_bytes->size(); ++diff) {
        if ((*in_bytes)[diff] != (*out_bytes)[diff]) {
            break;
        }
    }
    std::cerr << "Mismatch at byte " << diff << " (in=" << (int)(*in_bytes)[diff]
              << " out=" << (int)(*out_bytes)[diff] << ")\n";
    std::cerr << "Mismatch: audio payload differs (input " << in_bytes->size() << " bytes, output "
              << out_bytes->size() << " bytes)\n";
    return 1;
}
