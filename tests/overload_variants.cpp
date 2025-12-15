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
    // For image-bearing overloads we reuse the small test image next to the cover.
    std::string image_path = (std::filesystem::path(cover).parent_path() / "small1.jpg").string();

    if (!std::filesystem::exists(input)) {
        std::cout << "[overload_variants] skipping: missing input fixture " << input << "\n";
        return 0;
    }
    if (!cover.empty() && !std::filesystem::exists(cover)) {
        std::cout << "[overload_variants] skipping: missing cover fixture " << cover << "\n";
        return 0;
    }
    if (!std::filesystem::exists(image_path)) {
        std::cout << "[overload_variants] skipping: missing image fixture " << image_path << "\n";
        return 0;
    }

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

    // 3) Titles + images (no URL track).
    std::vector<uint8_t> img_bytes = load_file(image_path);
    std::vector<ChapterImageSample> images = {
        {.data = img_bytes, .start_ms = 0},
        {.data = img_bytes, .start_ms = 5000},
    };
    std::string out_img = (std::filesystem::path(out_dir) / "overload_img.m4a").string();
    ok = mux_file_to_m4a(input, titles, images, meta, out_img, true);
    if (!ok) {
        std::cerr << "mux (titles+images) failed\n";
        return 1;
    }
    if (!std::filesystem::exists(out_img) || std::filesystem::file_size(out_img) == 0) {
        std::cerr << "output missing or empty: " << out_img << "\n";
        return 1;
    }

    // 4) Titles + URLs + images, metadata empty (ilst reuse).
    std::string out_url_img =
        (std::filesystem::path(out_dir) / "overload_url_img.m4a").string();
    ok = mux_file_to_m4a(input, titles, urls, images, MetadataSet{}, out_url_img, true);
    if (!ok) {
        std::cerr << "mux (titles+urls+images) failed\n";
        return 1;
    }
    if (!std::filesystem::exists(out_url_img) ||
        std::filesystem::file_size(out_url_img) == 0) {
        std::cerr << "output missing or empty: " << out_url_img << "\n";
        return 1;
    }

    // 5) Titles + URLs + images via the 5-arg overload (metadata reuse).
    std::string out_url_img_nometa =
        (std::filesystem::path(out_dir) / "overload_url_img_nometa.m4a").string();
    ok = mux_file_to_m4a(input, titles, urls, images, out_url_img_nometa, true);
    if (!ok) {
        std::cerr << "mux (titles+urls+images 5-arg) failed\n";
        return 1;
    }
    if (!std::filesystem::exists(out_url_img_nometa) ||
        std::filesystem::file_size(out_url_img_nometa) == 0) {
        std::cerr << "output missing or empty: " << out_url_img_nometa << "\n";
        return 1;
    }

    // 6) Titles + images (no URL track), metadata reused (4-arg overload).
    std::string out_img_nometa =
        (std::filesystem::path(out_dir) / "overload_img_nometa.m4a").string();
    ok = mux_file_to_m4a(input, titles, images, out_img_nometa, true);
    if (!ok) {
        std::cerr << "mux (titles+images, 4-arg) failed\n";
        return 1;
    }
    if (!std::filesystem::exists(out_img_nometa) ||
        std::filesystem::file_size(out_img_nometa) == 0) {
        std::cerr << "output missing or empty: " << out_img_nometa << "\n";
        return 1;
    }

    // 7) JSON-path overload (includes optional url_text field).
    std::string json_path = (std::filesystem::path(out_dir) / "overload_inline.json").string();
    {
        std::ofstream jf(json_path);
        jf << R"({
  "title": "JSON Overload",
  "artist": "ChapterForge",
  "chapters": [
    { "start_ms": 0, "title": "J One", "url": "https://json.test/one", "url_text": "json-urltext-1" },
    { "start_ms": 5000, "title": "J Two", "url": "https://json.test/two", "url_text": "json-urltext-2" }
  ]
})";
    }
    std::string out_json =
        (std::filesystem::path(out_dir) / "overload_json.m4a").string();
    ok = mux_file_to_m4a(input, json_path, out_json, true);
    if (!ok) {
        std::cerr << "mux (json overload) failed\n";
        return 1;
    }
    if (!std::filesystem::exists(out_json) || std::filesystem::file_size(out_json) == 0) {
        std::cerr << "output missing or empty: " << out_json << "\n";
        return 1;
    }

    std::cout << "overload variants OK\n";
    return 0;
}
