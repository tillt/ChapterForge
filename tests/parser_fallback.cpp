#include "parser.hpp"
#include "logging.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

// Regression: parse_mp4 should succeed (with fallback if needed) on our fixture.
int main() {
    const std::string path = "testdata/input.m4a";
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
