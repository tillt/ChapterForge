//
//  chapter_timing.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

#include "logging.hpp"

// Derive durations (ms) from sorted start times. If total_ms > 0, the final
// duration is clamped to fill the remaining time up to total_ms (min 1).
template <typename Sample>
inline std::vector<uint32_t> derive_durations_ms_from_starts(const std::vector<Sample> &samples,
                                                             uint32_t total_ms = 0) {
    std::vector<uint32_t> durations;
    durations.reserve(samples.size());
    if (samples.empty()) {
        return durations;
    }
    if (samples.front().start_ms != 0) {
        // Apple players expect the first chapter to start at t=0. Warn callers so they
        // can surface this to the user (e.g., log a warning).
        CH_LOG("warn", "first chapter start_ms is " << samples.front().start_ms
                                                    << "ms; Apple players expect 0ms. "
                                                    << "Titles/URLs/thumbnails may not show.");
    }
    for (size_t i = 0; i < samples.size(); ++i) {
        if (i + 1 < samples.size()) {
            uint32_t cur = samples[i].start_ms;
            uint32_t next = samples[i + 1].start_ms;
            durations.push_back(next > cur ? (next - cur) : 1);
        } else {
            if (total_ms > 0 && samples[i].start_ms < total_ms) {
                durations.push_back(std::max<uint32_t>(1, total_ms - samples[i].start_ms));
            } else {
                durations.push_back(1);  // pad final sample with minimum duration
            }
        }
    }
    return durations;
}
