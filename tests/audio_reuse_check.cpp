// Compares the audio media payload extracted via stsz/stco/stsc between input and output MP4/M4A
// files.
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <limits>
#include <vector>

#include "parser.hpp"
#include "test_utils.hpp"

static std::optional<std::vector<uint8_t>> extract_audio_bytes(const std::string &path) {
    auto parsed_opt = parse_mp4(path);
    if (!parsed_opt) {
        std::cerr << "[reuse] parse failed for " << path << "\n";
        return std::nullopt;
    }
    const ParsedMp4 &parsed = *parsed_opt;
    auto sizes_opt = test_utils::parse_stsz_sizes(parsed.stsz);
    if (!sizes_opt || parsed.stco.empty() || parsed.stsc.empty()) {
        return std::nullopt;
    }
    const auto &sizes = *sizes_opt;

    std::vector<uint32_t> chunk_plan =
        test_utils::derive_chunk_plan(parsed.stsc, static_cast<uint32_t>(sizes.size()));
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
    uint64_t total_est = 0;
    for (auto s : sizes) {
        total_est += s;
    }
    out.reserve(static_cast<size_t>(
        std::min<uint64_t>(total_est, std::numeric_limits<size_t>::max())));

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
