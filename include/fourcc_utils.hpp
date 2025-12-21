//
//  fourcc_utils.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

// FourCC helpers.
inline constexpr uint32_t fourcc(const char a, const char b, const char c, const char d) {
    return (uint32_t(uint8_t(a)) << 24) | (uint32_t(uint8_t(b)) << 16) |
           (uint32_t(uint8_t(c)) << 8) | (uint32_t(uint8_t(d)));
}

inline constexpr uint32_t fourcc(const char t[4]) { return fourcc(t[0], t[1], t[2], t[3]); }

inline uint32_t fourcc(const std::string &s) {
    if (s.size() < 4) {
        throw std::runtime_error("fourcc string too short");
    }
    return fourcc(s[0], s[1], s[2], s[3]);
}

inline bool is_printable_fourcc(uint32_t type) {
    for (int i = 0; i < 4; ++i) {
        const uint8_t c = static_cast<uint8_t>(type >> (24 - 8 * i));
        if (c < 0x20 || c > 0x7E) {
            return false;
        }
    }
    return true;
}

inline std::string fourcc_to_string(uint32_t type) {
    std::string s(4, ' ');
    s[0] = static_cast<char>((type >> 24) & 0xFF);
    s[1] = static_cast<char>((type >> 16) & 0xFF);
    s[2] = static_cast<char>((type >> 8) & 0xFF);
    s[3] = static_cast<char>(type & 0xFF);
    return s;
}
