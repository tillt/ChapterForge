//
//  logging.hpp.
//  ChapterForge.
//
//  Minimal logging helper to stamp messages with origin and library name.
//

#pragma once

#include <cerrno>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>

inline void ch_log_impl(const char* level, const std::string& msg, const char* file, int line,
                        const char* func) {
    std::cerr << "[ChapterForge][" << level << "][" << file << ":" << line << " " << func << "] "
              << msg << std::endl;
}

#define CH_LOG(level, message)                                              \
    do {                                                                    \
        std::ostringstream _ch_log_ss;                                      \
        _ch_log_ss << message;                                              \
        ch_log_impl(level, _ch_log_ss.str(), __FILE__, __LINE__, __func__); \
    } while (0)
