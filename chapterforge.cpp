#include "chapterforge.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>

#include <nlohmann/json.hpp>

#include "parser.hpp"

using json = nlohmann::json;

namespace {

static bool read_file(const std::string &path, std::vector<uint8_t> &out) {
  std::ifstream f(path, std::ios::binary);
  if (!f.is_open())
    return false;
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
  for (auto &c : ext)
    c = static_cast<char>(::tolower(c));
  if (ext == ".m4a" || ext == ".mp4") {
    return extract_from_mp4(path);
  }
  std::vector<uint8_t> bytes;
  if (!read_file(path, bytes))
    return std::nullopt;
  auto res = extract_adts_frames(bytes);
  if (res.frames.empty())
    return std::nullopt;
  return res;
}

static bool load_chapters_json(const std::string &json_path,
                               std::vector<ChapterTextSample> &texts,
                               std::vector<ChapterImageSample> &images,
                               MetadataSet &meta) {
  std::ifstream f(json_path);
  if (!f.is_open())
    return false;
  json j;
  f >> j;
  auto resolve_path = [&](const std::string &p) {
    auto base = std::filesystem::path(json_path).parent_path();
    return (p.empty() ? std::filesystem::path() : base / p).string();
  };
  if (j.contains("chapters") && j["chapters"].is_array()) {
    for (auto &c : j["chapters"]) {
      ChapterTextSample t{};
      t.text = c.value("title", "");
      t.duration_ms = c.value("duration_ms", 0);
      texts.push_back(t);
      std::string image_path = c.value("image", "");
      if (!image_path.empty()) {
        ChapterImageSample im{};
        im.data = load_jpeg(resolve_path(image_path));
        im.duration_ms = t.duration_ms;
        images.push_back(std::move(im));
      }
    }
  }
  meta.title = j.value("title", "");
  meta.artist = j.value("artist", "");
  meta.album = j.value("album", "");
  meta.genre = j.value("genre", "");
  meta.year = j.value("year", "");
  meta.comment = j.value("comment", "");
  std::string cover_path = j.value("cover", "");
  if (!cover_path.empty())
    meta.cover = load_jpeg(resolve_path(cover_path));
  return true;
}

static bool metadata_is_empty(const MetadataSet &m) {
  return m.title.empty() && m.artist.empty() && m.album.empty() &&
         m.genre.empty() && m.year.empty() && m.comment.empty() &&
         m.cover.empty();
}

} // namespace

namespace mp4chapters {

bool mux_file_to_m4a(const std::string &input_audio_path,
                     const std::string &chapter_json_path,
                     const std::string &output_path) {
  auto aac = load_audio(input_audio_path);
  if (!aac) {
    std::cerr << "Failed to load audio from " << input_audio_path << "\n";
    return false;
  }

  std::vector<ChapterTextSample> text_chapters;
  std::vector<ChapterImageSample> image_chapters;
  MetadataSet meta;
  if (!load_chapters_json(chapter_json_path, text_chapters, image_chapters,
                          meta)) {
    std::cerr << "Failed to load chapters JSON: " << chapter_json_path << "\n";
    return false;
  }

  Mp4aConfig cfg{};
  const std::vector<uint8_t> *ilst_ptr = nullptr;
  if (!aac->ilst_payload.empty()) {
    ilst_ptr = &aac->ilst_payload;
    std::cerr << "[meta] Reusing source ilst metadata (" << ilst_ptr->size()
              << " bytes)\n";
  } else if (metadata_is_empty(meta)) {
    std::cerr << "[meta] WARNING: source metadata missing and no metadata provided; output will carry empty ilst\n";
  } else {
    std::cerr << "[meta] Using metadata provided by JSON overrides\n";
  }
  return write_mp4(output_path, *aac, text_chapters, image_chapters, cfg, meta,
                   /*fast_start=*/true, ilst_ptr);
}

bool mux_file_to_m4a(const std::string &input_audio_path,
                     const std::vector<ChapterTextSample> &text_chapters,
                     const std::vector<ChapterImageSample> &image_chapters,
                     const MetadataSet &metadata,
                     const std::string &output_path) {
  auto aac = load_audio(input_audio_path);
  if (!aac) {
    std::cerr << "Failed to load audio from " << input_audio_path << "\n";
    return false;
  }
  Mp4aConfig cfg{};
  const std::vector<uint8_t> *ilst_ptr = nullptr;
  if (!aac->ilst_payload.empty()) {
    ilst_ptr = &aac->ilst_payload;
    std::cerr << "[meta] Reusing source ilst metadata (" << ilst_ptr->size()
              << " bytes)\n";
  } else if (metadata_is_empty(metadata)) {
    std::cerr << "[meta] WARNING: source metadata missing and no metadata provided; output will carry empty ilst\n";
  } else {
    std::cerr << "[meta] Using metadata provided by caller\n";
  }
  return write_mp4(output_path, *aac, text_chapters, image_chapters, cfg,
                   metadata, /*fast_start=*/true, ilst_ptr);
}

} // namespace mp4chapters
