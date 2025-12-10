//
//  chapter_timing.hpp.
//  ChapterForge.
//
//  Utility to derive per-chapter durations from absolute start times.
//

#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

template <typename Sample>
inline std::vector<uint32_t> derive_durations_ms_from_starts(const std::vector<Sample> &samples) {
    std::vector<uint32_t> durations;
    durations.reserve(samples.size());
    for (size_t i = 0; i < samples.size(); ++i) {
        if (i + 1 < samples.size()) {
            uint32_t cur = samples[i].start_ms;
            uint32_t next = samples[i + 1].start_ms;
            durations.push_back(next > cur ? (next - cur) : 0);
        } else {
            durations.push_back(0);
        }
    }
    return durations;
}
