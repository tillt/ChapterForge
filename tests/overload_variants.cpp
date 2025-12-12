// Validates the public mux overloads: with metadata/no images, with URL track.
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include "chapterforge.hpp"
#include "meta_builder.hpp"

using namespace chapterforge;

static std::vector<uint8_t> load_file(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        return {};
    }
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
}

int main(int argc, char **argv) {
    if (argc < 4) {
        std::cerr << "usage: overload_variants <input.m4a> <cover.jpg> <out_dir>\n";
        return 2;
    }
    std::string input = argv[1];
    std::string cover = argv[2];
    std::string out_dir = argv[3];

    std::filesystem::create_directories(out_dir);

    // Common text chapters (two chapters at 0ms and 5s).
    std::vector<ChapterTextSample> titles = {
        {.text = "One", .href = "", .start_ms = 0},
        {.text = "Two", .href = "", .start_ms = 5000},
    };

    // URLs aligned with the titles (exercise URL-track overload).
    std::vector<ChapterTextSample> urls = {
        {.text = "", .href = "https://example.com/one", .start_ms = 0},
        {.text = "", .href = "https://example.com/two", .start_ms = 5000},
    };

    // Metadata for the first test (with cover).
    MetadataSet meta{};
    meta.title = "Overload Test";
    meta.artist = "ChapterForge";
    meta.album = "Tests";
    meta.comment = "Mux overload validation";
    meta.cover = load_file(cover);

    // 1) Titles + metadata, no images.
    std::string out_noimg = (std::filesystem::path(out_dir) / "overload_noimg.m4a").string();
    bool ok = mux_file_to_m4a(input, titles, meta, out_noimg, true);
    if (!ok) {
        std::cerr << "mux (no images) failed\n";
        return 1;
    }
    if (!std::filesystem::exists(out_noimg) || std::filesystem::file_size(out_noimg) == 0) {
        std::cerr << "output missing or empty: " << out_noimg << "\n";
        return 1;
    }

    // 2) Titles + URLs (no images), metadata empty (reuse source ilst if present).
    std::string out_url = (std::filesystem::path(out_dir) / "overload_url.m4a").string();
    ok = mux_file_to_m4a(input, titles, urls, std::vector<ChapterImageSample>{}, MetadataSet{},
                         out_url, true);
    if (!ok) {
        std::cerr << "mux (urls) failed\n";
        return 1;
    }
    if (!std::filesystem::exists(out_url) || std::filesystem::file_size(out_url) == 0) {
        std::cerr << "output missing or empty: " << out_url << "\n";
        return 1;
    }

    std::cout << "overload variants OK\n";
    return 0;
}
