//
//  main.cpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "chapterforge.hpp"
#include "chapterforge_version.hpp"
#include "logging.hpp"
#include <nlohmann/json.hpp>

chapterforge::LogVerbosity parse_level(const std::string &s) {
    if (s == "debug") return chapterforge::LogVerbosity::Debug;
    if (s == "info") return chapterforge::LogVerbosity::Info;
    if (s == "warn" || s == "warning") return chapterforge::LogVerbosity::Warn;
    return chapterforge::LogVerbosity::Error;
}

bool write_bytes(const std::filesystem::path &p, const std::vector<uint8_t> &data) {
    if (data.empty()) return false;
    std::ofstream out(p, std::ios::binary);
    if (!out.is_open()) return false;
    out.write(reinterpret_cast<const char *>(data.data()),
              static_cast<std::streamsize>(data.size()));
    return out.good();
}

bool emit_json(const chapterforge::ReadResult &res, const std::filesystem::path &image_dir) {
    nlohmann::json j;
    const auto &m = res.metadata;
    j["title"] = m.title;
    j["artist"] = m.artist;
    j["album"] = m.album;
    j["genre"] = m.genre;
    j["year"] = m.year;
    j["comment"] = m.comment;

    // Optionally export images (cover + chapter images) and reference them in JSON.
    std::vector<std::string> image_paths;
    if (!image_dir.empty()) {
        std::filesystem::create_directories(image_dir);
        // Cover.
        if (!m.cover.empty()) {
            auto cover_path = image_dir / "cover.jpg";
            if (write_bytes(cover_path, m.cover)) {
                j["cover"] = cover_path.string();
            }
        }
        // Chapter images.
        for (size_t i = 0; i < res.images.size(); ++i) {
            auto filename = "chapter" + std::to_string(i + 1) + ".jpg";
            auto path = image_dir / filename;
            if (write_bytes(path, res.images[i].data)) {
                image_paths.push_back(path.string());
            } else {
                image_paths.push_back("");
            }
        }
    }

    nlohmann::json chapters = nlohmann::json::array();
    size_t count = res.titles.size();
    for (size_t i = 0; i < count; ++i) {
        nlohmann::json c;
        const auto &t = res.titles[i];
        c["start_ms"] = t.start_ms;
        c["title"] = t.text;

        // Track URL and URL text only when present.
        std::string url_value;
        std::string url_text_value;
        if (i < res.urls.size()) {
            url_text_value = res.urls[i].text;
            if (!res.urls[i].href.empty()) {
                url_value = res.urls[i].href;
            }
        }
        // Fall back to title track href if URL track did not provide one.
        if (url_value.empty() && !t.href.empty()) {
            url_value = t.href;
        }
        if (!url_value.empty()) {
            c["url"] = url_value;
        }
        if (!url_text_value.empty()) {
            c["url_text"] = url_text_value;
        }
        if (!image_paths.empty() && i < image_paths.size()) {
            c["image"] = image_paths[i];
        }
        chapters.push_back(c);
    }
    j["chapters"] = chapters;

    std::cout << j.dump(2) << "\n";
    return true;
}

int main(int argc, char **argv) {
    if (argc == 2 && (std::string(argv[1]) == "--version" || std::string(argv[1]) == "-v")) {
        std::cout << "ChapterForge " << CHAPTERFORGE_VERSION_DISPLAY << "\n";
        return 0;
    }

    // Gather positional arguments (non-option).
    std::vector<std::string> positional;
    std::filesystem::path export_dir;
    bool fast_start = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--faststart") {
            fast_start = true;
        } else if (arg == "--log-level" && i + 1 < argc) {
            chapterforge::set_log_verbosity(parse_level(argv[i + 1]));
            ++i;
        } else if (arg == "--export-jpegs" && i + 1 < argc) {
            export_dir = argv[++i];
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << "\n";
            return 2;
        } else {
            positional.emplace_back(std::move(arg));
        }
    }

    if (positional.empty()) {
        std::cerr << "ChapterForge " << CHAPTERFORGE_VERSION_DISPLAY << "\n"
                  << "Copyright (c) 2025 Till Toenshoff\n\n"
                  << "usage for reading:\n"
                  << "  chapterforge <input.m4a> [--export-jpegs DIR] "
                  << "[--log-level warn|info|debug]\n"
                  << "usage for writing:\n"
                  << "  chapterforge <input.aac|input.m4a> <chapters.json> <output.m4a> "
                  << "[--faststart] [--log-level warn|info|debug]\n"
                  << "Options:\n"
                  << "  --faststart         Place 'moov' atom before 'mdat' for faster playback start.\n"
                  << "  --log-level LEVEL   Set logging verbosity (default: info).\n"
                  << "  --export-jpegs DIR  When reading, write chapter images (and cover if any) to DIR.\n"
                  << "                      JSON is always written to stdout when reading.\n";
        return 2;
    }

    // Reading mode: one positional argument (input).
    if (positional.size() == 1) {
        const std::string input_path = positional[0];
        auto res = chapterforge::read_m4a(input_path);
        if (!res.status.ok) {
            CH_LOG("error", "chapterforge: failed to read m4a: " << res.status.message);
            return 1;
        }
        if (!emit_json(res, export_dir)) {
            CH_LOG("error", "chapterforge: failed to emit JSON or export images");
            return 1;
        }
        return 0;
    }

    // Writing mode: three positional arguments.
    if (positional.size() != 3) {
        std::cerr << "Invalid arguments. See --help for usage.\n";
        return 2;
    }
    const std::string input_path = positional[0];
    const std::string chapters_path = positional[1];
    const std::string output_path = positional[2];

    auto status =
        chapterforge::mux_file_to_m4a(input_path, chapters_path, output_path, fast_start);
    if (!status.ok) {
        CH_LOG("error", "chapterforge: failed to mux m4a: " << status.message);
        return 1;
    }

    std::cout << "Wrote: " << output_path << "\n";
    return 0;
}
