//
//  logging.cpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#include "logging.hpp"

namespace chapterforge {

static std::atomic<int> g_log_level{static_cast<int>(LogVerbosity::Info)};

void set_log_verbosity(LogVerbosity level) {
    g_log_level.store(static_cast<int>(level), std::memory_order_relaxed);
}

LogVerbosity get_log_verbosity() {
    return static_cast<LogVerbosity>(g_log_level.load(std::memory_order_relaxed));
}

}  // namespace chapterforge
