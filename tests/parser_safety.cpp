#include "parser.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void write_u32(std::ofstream &out, uint32_t v) {
    uint8_t b[4] = {static_cast<uint8_t>((v >> 24) & 0xFF), static_cast<uint8_t>((v >> 16) & 0xFF),
                    static_cast<uint8_t>((v >> 8) & 0xFF), static_cast<uint8_t>(v & 0xFF)};
    out.write(reinterpret_cast<char *>(b), 4);
}

// Build a file whose first atom claims an absurd payload (> safety bound). The parser should
// refuse that size and still return without crashing.
bool make_huge_atom_file(const std::filesystem::path &p) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    const uint32_t huge_size = 0x20000010;  // payload >512MB; triggers size guard
    write_u32(out, huge_size);
    out.write("mdat", 4);
    // No payload is written; size guard in parser will zero the atom and bail.
    return true;
}

// Build a moov/trak hierarchy where the child overflows its parent. Parser should clamp and exit.
bool make_overflow_child_file(const std::filesystem::path &p) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    // moov size 32 bytes, child claims 40 -> will be clamped to fit.
    write_u32(out, 32);
    out.write("moov", 4);
    write_u32(out, 40);
    out.write("trak", 4);
    // Fill remaining bytes with zeros.
    char padding[32] = {};
    out.write(padding, sizeof(padding));
    return true;
}

int run_case(const std::filesystem::path &p, bool (*builder)(const std::filesystem::path &)) {
    if (!builder(p)) {
        std::cerr << "[parser_safety] failed to build fixture at " << p << "\n";
        return 2;
    }
    auto parsed = parse_mp4(p.string());
    if (!parsed) {
        std::cerr << "[parser_safety] parse_mp4 returned nullopt for " << p << "\n";
        return 3;
    }
    // We only care that parsing completed without blowing up. Sample tables may be empty.
    return 0;
}

}  // namespace

int main() {
    const auto tmp = std::filesystem::temp_directory_path();
    const auto huge_atom = tmp / "parser_huge_atom.mp4";
    const auto overflow = tmp / "parser_overflow_child.mp4";

    int rc = run_case(huge_atom, make_huge_atom_file);
    if (rc != 0) {
        return rc;
    }
    rc = run_case(overflow, make_overflow_child_file);
    return rc;
}
