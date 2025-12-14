// Fails if any referenced image/cover files in testdata JSON are missing.
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

static bool load_json(const std::string &path, json &out) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "[image_fixtures] missing json: " << path << "\n";
        return false;
    }
    try {
        f >> out;
    } catch (const std::exception &e) {
        std::cerr << "[image_fixtures] parse error in " << path << ": " << e.what() << "\n";
        return false;
    }
    return true;
}

static bool check_path(const std::filesystem::path &root, const std::string &rel) {
    std::filesystem::path p = root / rel;
    if (!std::filesystem::exists(p)) {
        std::cerr << "[image_fixtures] missing file: " << p.string() << "\n";
        return false;
    }
    return true;
}

int main(int argc, char **argv) {
    std::filesystem::path root = "testdata";
    if (argc > 1) {
        root = argv[1];
    }

    bool ok = true;
    for (const auto &entry :
        {"chapters.json", "chapters_10s_2ch_normalimg_meta.json",
         "chapters_10s_2ch_normalimg_nometa.json", "chapters_250s_50ch_largeimg_meta.json"}) {
        json j;
        if (!load_json((root / entry).string(), j)) {
            ok = false;
            continue;
        }
        if (j.contains("cover") && j["cover"].is_string()) {
            ok = check_path(root, j["cover"].get<std::string>()) && ok;
        }
        if (j.contains("chapters") && j["chapters"].is_array()) {
            for (const auto &c : j["chapters"]) {
                if (c.contains("image") && c["image"].is_string()) {
                    ok = check_path(root, c["image"].get<std::string>()) && ok;
                }
            }
        }
    }
    return ok ? 0 : 1;
}
