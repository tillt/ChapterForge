#include "chapterforge.hpp"
#include "chapterforge_version.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>
#include <utility>

#include "logging.hpp"
#include "parser.hpp"

using json = nlohmann::json;

namespace {

static bool read_file(const std::string &path, std::vector<uint8_t> &out) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        CH_LOG("io", "open failed for " << path << " errno=" << errno << " ("
                                        << std::generic_category().message(errno) << ")");
        return false;
    }
    f.seekg(0, std::ios::end);
    size_t sz = static_cast<size_t>(f.tellg());
    f.seekg(0, std::ios::beg);
    out.resize(sz);
    f.read(reinterpret_cast<char *>(out.data()), static_cast<std::streamsize>(sz));
    return true;
}

static std::vector<uint8_t> load_jpeg(const std::string &path) {
    std::vector<uint8_t> out;
    read_file(path, out);
    return out;
}

static std::optional<AacExtractResult> load_audio(const std::string &path) {
    // If the input is M4A/MP4, reuse stbl; if ADTS, decode to frames.
    auto ext = std::filesystem::path(path).extension().string();
    for (auto &c : ext) {
        c = static_cast<char>(::tolower(c));
    }
    if (ext == ".m4a" || ext == ".mp4") {
        return extract_from_mp4(path);
    }
    std::vector<uint8_t> bytes;
    if (!read_file(path, bytes)) {
        return std::nullopt;
    }
    auto res = extract_adts_frames(bytes);
    if (res.frames.empty()) {
        return std::nullopt;
    }
    return res;
}

struct PendingChapter {
    std::string title;
    uint32_t start_ms = 0;
    std::string image_path;
    std::string url;
};

static bool load_chapters_json(
    const std::string &json_path, std::vector<ChapterTextSample> &texts,
    std::vector<ChapterImageSample> &images, MetadataSet &meta,
    std::vector<std::pair<std::string, std::vector<ChapterTextSample>>> &extra_text_tracks) {
    std::ifstream f(json_path);
    if (!f.is_open()) {
        CH_LOG("io", "open failed for " << json_path << " errno=" << errno << " ("
                                        << std::generic_category().message(errno) << ")");
        return false;
    }
    json j;
    f >> j;
    auto resolve_path = [&](const std::string &p) {
        auto base = std::filesystem::path(json_path).parent_path();
        return (p.empty() ? std::filesystem::path() : base / p).string();
    };
    std::vector<PendingChapter> pending;
    if (j.contains("chapters") && j["chapters"].is_array()) {
        pending.reserve(j["chapters"].size());
        for (auto &c : j["chapters"]) {
            PendingChapter p{};
            p.title = c.value("title", "");
            p.start_ms = c.value("start_ms", 0);
            p.image_path = c.value("image", "");
            p.url = c.value("url", "");
            pending.push_back(std::move(p));
        }
    }

    std::vector<ChapterTextSample> url_chapters;
    bool any_url = false;
    for (const auto &p : pending) {
        ChapterTextSample t{};
        t.text = p.title;
        t.start_ms = p.start_ms;
        ChapterTextSample url_sample{};
        url_sample.start_ms = p.start_ms;
        // Keep URL track text empty; golden URL tracks can carry minimal/empty text.
        url_sample.text = "";
        if (!p.url.empty()) {
            any_url = true;
            url_sample.href = p.url;
        }
        texts.push_back(t);
        url_chapters.push_back(std::move(url_sample));

        if (!p.image_path.empty()) {
            ChapterImageSample im{};
            im.data = load_jpeg(resolve_path(p.image_path));
            im.start_ms = t.start_ms;
            images.push_back(std::move(im));
        }
    }
    if (any_url) {
        extra_text_tracks.push_back({"Chapter URLs", std::move(url_chapters)});
    }
    meta.title = j.value("title", "");
    meta.artist = j.value("artist", "");
    meta.album = j.value("album", "");
    meta.genre = j.value("genre", "");
    meta.year = j.value("year", "");
    meta.comment = j.value("comment", "");
    std::string cover_path = j.value("cover", "");
    if (!cover_path.empty()) {
        meta.cover = load_jpeg(resolve_path(cover_path));
    }
    return true;
}

static bool metadata_is_empty(const MetadataSet &m) {
    return m.title.empty() && m.artist.empty() && m.album.empty() && m.genre.empty() &&
           m.year.empty() && m.comment.empty() && m.cover.empty();
}

}  // namespace

namespace chapterforge {

bool mux_file_to_m4a(const std::string &input_audio_path, const std::string &chapter_json_path,
                     const std::string &output_path, bool fast_start) {
    CH_LOG("info", "ChapterForge version " << CHAPTERFORGE_VERSION_DISPLAY);
    auto aac = load_audio(input_audio_path);
    if (!aac) {
        CH_LOG("error", "Failed to load audio from " << input_audio_path);
        return false;
    }

    std::vector<ChapterTextSample> text_chapters;
    std::vector<ChapterImageSample> image_chapters;
    std::vector<std::pair<std::string, std::vector<ChapterTextSample>>> extra_text_tracks;
    MetadataSet meta;
    if (!load_chapters_json(chapter_json_path, text_chapters, image_chapters, meta,
                            extra_text_tracks)) {
        CH_LOG("error", "Failed to load chapters JSON: " << chapter_json_path);
        return false;
    }

    Mp4aConfig cfg{};
    const std::vector<uint8_t> *ilst_ptr = nullptr;
    if (!aac->ilst_payload.empty()) {
        ilst_ptr = &aac->ilst_payload;
        CH_LOG("meta", "Reusing source ilst metadata (" << ilst_ptr->size() << " bytes)");
    } else if (metadata_is_empty(meta)) {
        CH_LOG("meta",
               "WARNING: source metadata missing and no metadata "
               "provided; output will carry empty ilst");
    } else {
        CH_LOG("meta", "Using metadata provided by JSON overrides");
    }
    return write_mp4(output_path, *aac, text_chapters, image_chapters, cfg, meta, fast_start,
                     extra_text_tracks, ilst_ptr);
}

bool mux_file_to_m4a(const std::string &input_audio_path,
                     const std::vector<ChapterTextSample> &text_chapters,
                     const std::vector<ChapterImageSample> &image_chapters,
                     const MetadataSet &metadata, const std::string &output_path, bool fast_start) {
    CH_LOG("info", "ChapterForge version " << CHAPTERFORGE_VERSION_DISPLAY);
    auto aac = load_audio(input_audio_path);
    if (!aac) {
        CH_LOG("error", "Failed to load audio from " << input_audio_path);
        return false;
    }
    Mp4aConfig cfg{};
    const std::vector<uint8_t> *ilst_ptr = nullptr;
    if (!aac->ilst_payload.empty()) {
        ilst_ptr = &aac->ilst_payload;
        CH_LOG("meta", "Reusing source ilst metadata (" << ilst_ptr->size() << " bytes)");
    } else if (metadata_is_empty(metadata)) {
        CH_LOG("meta",
               "WARNING: source metadata missing and no metadata "
               "provided; output will carry empty ilst");
    } else {
        CH_LOG("meta", "Using metadata provided by caller");
    }
    std::vector<std::pair<std::string, std::vector<ChapterTextSample>>> extra_text_tracks;
    return write_mp4(output_path, *aac, text_chapters, image_chapters, cfg, metadata, fast_start,
                     extra_text_tracks, ilst_ptr);
}

bool mux_file_to_m4a(const std::string &input_audio_path,
                     const std::vector<ChapterTextSample> &text_chapters,
                     const MetadataSet &metadata, const std::string &output_path,
                     bool fast_start) {
    std::vector<ChapterImageSample> empty_images;
    return mux_file_to_m4a(input_audio_path, text_chapters, empty_images, metadata, output_path,
                           fast_start);
}

bool mux_file_to_m4a(const std::string &input_audio_path,
                     const std::vector<ChapterTextSample> &text_chapters,
                     const std::vector<ChapterImageSample> &image_chapters,
                     const std::string &output_path, bool fast_start) {
    MetadataSet empty{};
    return mux_file_to_m4a(input_audio_path, text_chapters, image_chapters, empty, output_path,
                           fast_start);
}

bool mux_file_to_m4a(const std::string &input_audio_path,
                     const std::vector<ChapterTextSample> &text_chapters,
                     const std::vector<ChapterTextSample> &url_chapters,
                     const std::vector<ChapterImageSample> &image_chapters,
                     const MetadataSet &metadata, const std::string &output_path, bool fast_start) {
    CH_LOG("info", "ChapterForge version " << CHAPTERFORGE_VERSION_DISPLAY);
    auto aac = load_audio(input_audio_path);
    if (!aac) {
        CH_LOG("error", "Failed to load audio from " << input_audio_path);
        return false;
    }
    Mp4aConfig cfg{};
    const std::vector<uint8_t> *ilst_ptr = nullptr;
    if (!aac->ilst_payload.empty()) {
        ilst_ptr = &aac->ilst_payload;
        CH_LOG("meta", "Reusing source ilst metadata (" << ilst_ptr->size() << " bytes)");
    } else if (metadata_is_empty(metadata)) {
        CH_LOG("meta",
               "WARNING: source metadata missing and no metadata "
               "provided; output will carry empty ilst");
    } else {
        CH_LOG("meta", "Using metadata provided by caller");
    }
    std::vector<std::pair<std::string, std::vector<ChapterTextSample>>> extra_text_tracks;
    if (!url_chapters.empty()) {
        extra_text_tracks.push_back({"Chapter URLs", url_chapters});
    }
    return write_mp4(output_path, *aac, text_chapters, image_chapters, cfg, metadata, fast_start,
                     extra_text_tracks, ilst_ptr);
}

}  // namespace chapterforge
