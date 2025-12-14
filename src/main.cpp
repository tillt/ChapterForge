//
//  main.cpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#include <iostream>
#include <string>

#include "chapterforge.hpp"
#include "chapterforge_version.hpp"
#include "logging.hpp"

chapterforge::LogVerbosity parse_level(const std::string &s) {
    if (s == "debug") return chapterforge::LogVerbosity::Debug;
    if (s == "info") return chapterforge::LogVerbosity::Info;
    if (s == "warn" || s == "warning") return chapterforge::LogVerbosity::Warn;
    return chapterforge::LogVerbosity::Error;
}

int main(int argc, char **argv) {
    if (argc == 2 && (std::string(argv[1]) == "--version" || std::string(argv[1]) == "-v")) {
        std::cout << "ChapterForge " << CHAPTERFORGE_VERSION_DISPLAY << "\n";
        return 0;
    }

    if (argc < 4 || argc > 7) {
        std::cerr
            << "usage: chapterforge <input.m4a|.mp4|.aac> <chapters.json> <output.m4a> "
            << "[--faststart] [--log-level warn|info|debug]\n";
        return 2;
    }

    const std::string input_path = argv[1];
    const std::string chapters_path = argv[2];
    const std::string output_path = argv[3];
    bool fast_start = false;

    for (int i = 4; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--faststart") {
            fast_start = true;
        } else if (arg == "--log-level" && i + 1 < argc) {
            chapterforge::set_log_verbosity(parse_level(argv[i + 1]));
            ++i;
        }
    }

    if (!chapterforge::mux_file_to_m4a(input_path, chapters_path, output_path, fast_start)) {
        CH_LOG("error", "chapterforge: failed to write output");
        return 1;
    }

    std::cout << "Wrote: " << output_path << "\n";
    return 0;
}
