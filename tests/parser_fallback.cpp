#include "parser.hpp"
#include "logging.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

// Regression: parse_mp4 should succeed (with fallback if needed) on our fixture.
int main() {
    const char *env_path = std::getenv("PARSER_TEST_MP4");
    const std::string path = env_path ? std::string(env_path) : std::string("testdata/input.m4a");

    // If the fixture is missing (CI without test assets), skip.
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        std::cout << "[parser_fallback_check] skipping: fixture not found at " << path << "\n";
        return 0;
    }

    auto parsed = parse_mp4(path);
    if (!parsed) {
        std::cerr << "parse_mp4 returned nullopt for " << path << "\n";
        return 1;
    }
    if (parsed->stco.empty() || parsed->stsc.empty() || parsed->stsz.empty() ||
        parsed->stsd.empty()) {
        std::cerr << "stbl atoms missing for " << path << "\n";
        return 2;
    }
    return 0;
}
