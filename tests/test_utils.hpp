// Minimal MP4 table parsing helpers for tests only (kept independent of the
// library code to avoid self-consistency bugs).
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace test_utils {

inline std::optional<std::vector<uint32_t>> parse_stsz_sizes(
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

inline std::vector<uint32_t> derive_chunk_plan(const std::vector<uint8_t> &stsc_payload,
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

}  // namespace test_utils
