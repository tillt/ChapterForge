// Unit test for the public read_m4a API: validates titles, URLs, and images are parsed.
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "chapterforge.hpp"
#include "logging.hpp"

#ifndef TESTDATA_DIR
#error "TESTDATA_DIR must be defined"
#endif

namespace {

bool check(bool cond, const std::string &msg) {
    if (!cond) {
        std::fprintf(stderr, "[read_unit] FAIL: %s\n", msg.c_str());
    }
    return cond;
}

template <typename T, typename Pred>
std::vector<T> non_empty(const std::vector<T> &in, Pred pred) {
    std::vector<T> out;
    for (const auto &v : in) {
        if (pred(v)) {
            out.push_back(v);
        }
    }
    return out;
}

bool test_read_output_debug() {
    const std::filesystem::path testdata(TESTDATA_DIR);
    const auto input = (testdata / "input.m4a").string();

    // Build a fresh output with titles, URL track and images so we can read it back.
    std::vector<ChapterTextSample> titles;
    ChapterTextSample t1{};
    t1.text = "Read Title One";
    t1.href = "https://chapterforge.test/url-one";
    t1.start_ms = 0;
    titles.push_back(t1);
    ChapterTextSample t2{};
    t2.text = "Read Title Two";
    t2.href = "https://chapterforge.test/url-two";
    t2.start_ms = 5000;
    titles.push_back(t2);

    std::vector<ChapterTextSample> urls;
    ChapterTextSample u1{};
    u1.text = "Read URL text one";
    u1.href = "https://chapterforge.test/url-one";
    u1.start_ms = 0;
    urls.push_back(u1);
    ChapterTextSample u2{};
    u2.text = "Read URL text two";
    u2.href = "https://chapterforge.test/url-two";
    u2.start_ms = 5000;
    urls.push_back(u2);

    auto load_bytes = [](const std::filesystem::path &p) {
        std::vector<uint8_t> data;
        std::FILE *f = std::fopen(p.string().c_str(), "rb");
        if (!f) {
            return data;
        }
        std::fseek(f, 0, SEEK_END);
        long len = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        if (len > 0) {
            data.resize(static_cast<size_t>(len));
            std::fread(data.data(), 1, static_cast<size_t>(len), f);
        }
        std::fclose(f);
        return data;
    };

    std::vector<ChapterImageSample> images;
    // Reuse existing 400x400 fixtures that we already ship with the repo.
    auto img1 = load_bytes(testdata / "images" / "chapter1.jpg");
    auto img2 = load_bytes(testdata / "images" / "chapter2.jpg");
    if (!img1.empty()) {
        ChapterImageSample im1{};
        im1.start_ms = 0;
        im1.data = std::move(img1);
        images.push_back(std::move(im1));
    }
    if (!img2.empty()) {
        ChapterImageSample im2{};
        im2.start_ms = 5000;
        im2.data = std::move(img2);
        images.push_back(std::move(im2));
    }

    const auto out_dir = std::filesystem::path("test_outputs");
    std::filesystem::create_directories(out_dir);
    const auto out_path = (out_dir / "read_unit.m4a").string();

    MetadataSet meta{};
    meta.title = "Read Unit Album";
    meta.artist = "ChapterForge Tester";
    meta.album = "Read Unit Suite";
    meta.genre = "Unit Jazz";
    meta.year = "2025";
    meta.comment = "Round-trip read API validation";
    meta.cover = load_bytes(testdata / "images" / "cover.jpg");

    auto mux_status =
        chapterforge::mux_file_to_m4a(input, titles, urls, images, meta, out_path, true);
    bool ok = true;
    ok &= check(mux_status.ok, "mux status ok");
    if (!mux_status.ok) {
        return false;
    }

    auto res = chapterforge::read_m4a(out_path);
    ok &= check(res.status.ok, "read_m4a ok");
    if (!res.status.ok) {
        return false;
    }

    ok &= check(res.metadata.title == meta.title, "metadata title");
    ok &= check(res.metadata.artist == meta.artist, "metadata artist");
    ok &= check(res.metadata.album == meta.album, "metadata album");
    ok &= check(res.metadata.genre == meta.genre, "metadata genre");
    ok &= check(res.metadata.year == meta.year, "metadata year");
    ok &= check(res.metadata.comment == meta.comment, "metadata comment");
    ok &= check(!res.metadata.cover.empty(), "metadata cover present");
    ok &= check(res.metadata.cover.size() == meta.cover.size(), "metadata cover size match");

    auto parsed_titles = non_empty(res.titles, [](const auto &t) { return !t.text.empty(); });
    ok &= check(parsed_titles.size() >= 2, "titles count >=2");
    if (parsed_titles.size() >= 2) {
        ok &= check(parsed_titles[0].text == "Read Title One", "title[0] text");
        ok &= check(parsed_titles[0].href == "https://chapterforge.test/url-one", "title[0] href");
        ok &= check(parsed_titles[1].text == "Read Title Two", "title[1] text");
        ok &= check(parsed_titles[1].href == "https://chapterforge.test/url-two", "title[1] href");
        ok &= check(parsed_titles[0].start_ms == 0, "title[0] start");
        ok &= check(parsed_titles[1].start_ms == 5000, "title[1] start");
    }

    auto parsed_urls = non_empty(res.urls, [](const auto &t) {
        return !t.text.empty() || !t.href.empty();
    });
    ok &= check(parsed_urls.size() >= 2, "urls count >=2");
    if (parsed_urls.size() >= 2) {
        ok &= check(parsed_urls[0].text == "Read URL text one", "url[0] text");
        ok &= check(parsed_urls[0].href == "https://chapterforge.test/url-one", "url[0] href");
        ok &= check(parsed_urls[1].text == "Read URL text two", "url[1] text");
        ok &= check(parsed_urls[1].href == "https://chapterforge.test/url-two", "url[1] href");
    }

    ok &= check(res.images.size() >= 2, "images count >=2");
    if (!res.images.empty()) {
        ok &= check(!res.images[0].data.empty(), "first image non-empty");
    }
    return ok;
}

bool test_read_fixture_metadata() {
    const std::filesystem::path testdata(TESTDATA_DIR);
    const auto input = (testdata / "input.m4a").string();

    auto res = chapterforge::read_m4a(input);
    bool ok = true;
    ok &= check(res.status.ok, "read_m4a on fixture ok");
    if (!res.status.ok) {
        return false;
    }
    ok &= check(res.metadata.title == "ChapterForge Sample 10s (Pads)", "fixture title");
    ok &= check(res.metadata.artist == "ChapterForge Bot", "fixture artist");
    ok &= check(res.metadata.album == "Synthetic Pad Chapters", "fixture album");
    ok &= check(res.metadata.genre == "Test Audio", "fixture genre");
    ok &= check(res.metadata.comment == "Synthetic pads with voiceover chapters",
                "fixture comment");
    ok &= check(!res.metadata.cover.empty(), "fixture cover present");
    return ok;
}

}  // namespace

int main() {
    chapterforge::set_log_verbosity(chapterforge::LogVerbosity::Debug);
    bool ok = true;
    ok &= test_read_output_debug();
    ok &= test_read_fixture_metadata();
    return ok ? 0 : 1;
}
