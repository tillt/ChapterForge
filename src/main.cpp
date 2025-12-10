//
//  main.cpp
//  ChapterForge CLI
//

#include <iostream>
#include <string>

#include "chapterforge.hpp"

int main(int argc, char **argv) {
  if (argc != 4) {
    std::cerr << "usage: chapterforge <input.m4a|.mp4|.aac> <chapters.json> <output.m4a>\n";
    return 2;
  }

  const std::string input_path = argv[1];
  const std::string chapters_path = argv[2];
  const std::string output_path = argv[3];

  if (!mp4chapters::mux_file_to_m4a(input_path, chapters_path, output_path)) {
    std::cerr << "chapterforge: failed to write output\n";
    return 1;
  }

  std::cout << "Wrote: " << output_path << "\n";
  return 0;
}
