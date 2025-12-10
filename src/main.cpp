//
//  main.cpp.
//  ChapterForge CLI.
//

#include <iostream>
#include <string>

#include "chapterforge.hpp"
#include "logging.hpp"

int main(int argc, char **argv) {
    if (argc < 4 || argc > 5) {
        CH_LOG(
            "usage",
            "usage: chapterforge <input.m4a|.mp4|.aac> <chapters.json> <output.m4a> [--faststart]");
        return 2;
    }

    const std::string input_path = argv[1];
    const std::string chapters_path = argv[2];
    const std::string output_path = argv[3];
    const bool fast_start = (argc == 5 && std::string(argv[4]) == "--faststart");

    if (!chapterforge::mux_file_to_m4a(input_path, chapters_path, output_path, fast_start)) {
        CH_LOG("error", "chapterforge: failed to write output");
        return 1;
    }

    std::cout << "Wrote: " << output_path << "\n";
    return 0;
}
