// Unit coverage for small helpers: endian readers, JPEG info parser, and hex preview.
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "jpeg_info.hpp"
#include "logging.hpp"
#include "parser.hpp"

namespace {

bool check(bool cond, const std::string &msg) {
    if (!cond) {
        std::cerr << "[helper_unit] FAIL: " << msg << "\n";
    }
    return cond;
}

std::optional<std::vector<uint8_t>> read_file(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        std::cerr << "[helper_unit] open failed for " << path << "\n";
        return std::nullopt;
    }
    f.seekg(0, std::ios::end);
    std::streamoff len = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(static_cast<size_t>(len));
    f.read(reinterpret_cast<char *>(buf.data()), len);
    if (f.gcount() != len) {
        std::cerr << "[helper_unit] short read for " << path << "\n";
        return std::nullopt;
    }
    return buf;
}

bool test_endian_readers() {
    std::istringstream s32(std::string("\x01\x02\x03\x04", 4));
    uint32_t v32 = read_u32(s32);
    bool ok = check(v32 == 0x01020304u, "read_u32 big-endian decode");

    std::istringstream s64(std::string("\x01\x02\x03\x04\x05\x06\x07\x08", 8));
    uint64_t v64 = read_u64(s64);
    ok &= check(v64 == 0x0102030405060708ULL, "read_u64 big-endian decode");
    return ok;
}

bool test_hex_prefix() {
    using chapterforge::hex_prefix;
    bool ok = check(hex_prefix({}) == "", "hex_prefix empty");
    std::vector<uint8_t> data = {0x00, 0x11, 0xAB, 0xCD, 0xFF};
    ok &= check(hex_prefix(data, 4) == "00 11 ab cd", "hex_prefix truncates to max_len");
    ok &= check(hex_prefix(data) == "00 11 ab cd ff", "hex_prefix default prints all up to limit");
    return ok;
}

bool test_jpeg_info(const std::string &jpeg_path) {
    auto buf_opt = read_file(jpeg_path);
    if (!buf_opt) {
        return false;
    }
    const auto &buf = *buf_opt;
    uint16_t w = 0, h = 0;
    bool is420 = false;
    bool ok = parse_jpeg_info(buf, w, h, is420);
    ok &= check(ok, "parse_jpeg_info returns true for valid JPEG");
    ok &= check(w == 400 && h == 400, "parse_jpeg_info returns expected dimensions (400x400)");
    ok &= check(is420, "parse_jpeg_info detects yuv420 sampling");

    // Negative: PNG signature should not parse as JPEG.
    std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    w = h = 123;
    is420 = true;
    bool ok_png = !parse_jpeg_info(png, w, h, is420);
    ok &= check(ok_png, "parse_jpeg_info rejects PNG signature");
    ok &= check(w == 123 && h == 123, "parse_jpeg_info leaves dimensions untouched on failure");
    return ok;
}

}  // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "usage: helper_unit <TESTDATA_DIR>\n";
        return 2;
    }
    const std::string testdata_dir = argv[1];
    bool ok = true;
    ok &= test_endian_readers();
    ok &= test_hex_prefix();
    ok &= test_jpeg_info(testdata_dir + "/images/chapter1.jpg");
    return ok ? 0 : 1;
}
