//
//  logging.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#pragma once

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <cstdint>

namespace chapterforge {

enum class LogVerbosity { Error = 0, Warn = 1, Info = 2, Debug = 3 };

// Set/get global logging verbosity.
void set_log_verbosity(LogVerbosity level);
LogVerbosity get_log_verbosity();

// Hex-preview helper used in debug logs to dump a short prefix of binary blobs (e.g. JPEG).
inline constexpr size_t kHexPreviewBytes = 8;
inline std::string hex_prefix(const std::vector<uint8_t>& data,
                              size_t max_len = kHexPreviewBytes) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    const size_t limit = std::min(max_len, data.size());
    for (size_t i = 0; i < limit; ++i) {
        oss << std::setw(2) << static_cast<unsigned int>(data[i]);
        if (i + 1 != limit) {
            oss << ' ';
        }
    }
    return oss.str();
}

}  // namespace chapterforge

inline constexpr chapterforge::LogVerbosity ch_severity_for_tag(std::string_view tag) {
    if (tag == "error") {
        return chapterforge::LogVerbosity::Error;
    }
    if (tag == "warn" || tag == "warning") {
        return chapterforge::LogVerbosity::Warn;
    }
    if (tag == "info") {
        return chapterforge::LogVerbosity::Info;
    }
    // Everything else (io/parser/meta/etc.) treated as debug-level.
    return chapterforge::LogVerbosity::Debug;
}

inline bool ch_should_log(const char* level) {
    const auto current = chapterforge::get_log_verbosity();
    const auto sev = ch_severity_for_tag(level ? level : "");
    return static_cast<int>(sev) <= static_cast<int>(current);
}

inline void ch_log_impl(const char* level, const std::string& msg, const char* file, int line,
                        const char* func) {
    std::string lvl(level ? level : "");
    if (lvl == "error") {
        std::cerr << "[ChapterForge][" << level << "][" << file << ":" << line << " " << func
                  << "] " << msg << std::endl;
    } else {
        std::cerr << "[ChapterForge][" << level << "] " << msg << std::endl;
    }
}

#define CH_LOG(level, message)                                              \
    do {                                                                    \
        if (ch_should_log(level)) {                                         \
            std::ostringstream _ch_log_ss;                                  \
            _ch_log_ss << message;                                          \
            ch_log_impl(level, _ch_log_ss.str(), __FILE__, __LINE__,        \
                        __func__);                                          \
        }                                                                   \
    } while (0)
