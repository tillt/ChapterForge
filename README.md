# ChapterForge

ChapterForge is a library and CLI to mux chapters (text and optional images) into AAC/M4A files while preserving metadata and handling Apple-compatible chapter tracks.

## Building

```bash
cmake -S . -B build
cmake --build build
```

Targets:
- `chapterforge` — static library
- `chapterforge_cli` — command-line tool

## CLI Usage

```bash
./chapterforge_cli <input.m4a|.mp4|.aac> <chapters.json> <output.m4a>
```

- If the input already has metadata (`ilst`), it is reused by default.
- Fast-start is on by default.

## Chapters JSON format

ChapterForge consumes a simple JSON document:

```jsonc
{
  "title": "Sample Podcast Episode",     // optional top-level metadata
  "artist": "John Doe",
  "album": "My Podcast",
  "genre": "Podcast",
  "year": "2024",
  "comment": "Created with ChapterForge",
  "cover": "cover.jpg",                  // optional; path is relative to the JSON file

  "chapters": [
    {
      "title": "Introduction",           // required
      "start_ms": 0,                     // required: chapter start time in milliseconds
      "image": "chapter1.jpg"            // optional; path relative to the JSON file
    },
    {
      "title": "Main Discussion",
      "start_ms": 10000,
      "image": "chapter2.jpg"
    }
  ]
}
```

Notes:
- Chapters are positioned by absolute start times (`start_ms`); the muxer converts these to durations internally.
- Chapter images are optional; omit `image` to create a text-only chapter.
- If top-level metadata fields are omitted and the input file already contains an `ilst`, the existing metadata is preserved automatically.
- Paths for `cover` and per-chapter `image` are resolved relative to the JSON file location.

## Embedding API (C++)

Public header: `chapterforge.hpp`

```c++
bool mux_file_to_m4a(const std::string& input_audio_path,
                     const std::string& chapter_json_path,
                     const std::string& output_path);

bool mux_file_to_m4a(const std::string& input_audio_path,
                     const std::vector<ChapterTextSample>& text_chapters,
                     const std::vector<ChapterImageSample>& image_chapters,
                     const MetadataSet& metadata,
                     const std::string& output_path);
```

If `metadata` is empty and the source has an `ilst`, it is reused automatically.

## Minimal C++ usage (CLI equivalent)

The CLI front-end is effectively:

```c++
#include "chapterforge.hpp"
#include <iostream>

int main(int argc, char** argv) {
  if (argc != 4) {
    std::cerr << "usage: chapterforge <input.m4a|.mp4|.aac> <chapters.json> <output.m4a>\n";
    return 2;
  }
  std::string input   = argv[1];
  std::string chapters= argv[2];
  std::string output  = argv[3];

  if (!mp4chapters::mux_file_to_m4a(input, chapters, output)) {
    std::cerr << "chapterforge: failed to write output\n";
    return 1;
  }
  std::cout << "Wrote: " << output << "\n";
  return 0;
}
```

Use the higher-level overload if you already have chapters/material in memory and don’t want to read JSON on disk.

## Objective-C++ Example

Invoking `write_mp4` from Objective-C++ using an `NSArray<NSDictionary*>` of chapters (`@"title"`: `NSString*`, `@"time"`: `NSNumber` in milliseconds):

```objective-c++
#import "chapterforge.hpp" // public header
#import <Foundation/Foundation.h>

static void BuildChaptersFromDict(NSArray<NSDictionary *> *chapterArray,
                                  std::vector<ChapterTextSample> &textChapters) {
    textChapters.clear();
    textChapters.reserve([chapterArray count]);
    for (NSDictionary *entry in chapterArray) {
        NSString *title = entry[@"title"];
        NSNumber *timeMs = entry[@"time"];
        if (!title || !timeMs) continue;
        ChapterTextSample s{};
        s.text = [title UTF8String];
        s.duration_ms = [timeMs unsignedIntValue]; // using duration_ms as start in ms
        textChapters.push_back(std::move(s));
    }
}

void ExampleMuxFromObjectiveC(NSArray<NSDictionary *> *chaptersDict,
                              const AacExtractResult &aac,
                              const std::string &outputPath) {
    std::vector<ChapterTextSample> textChapters;
    std::vector<ChapterImageSample> imageChapters; // empty if no images
    BuildChaptersFromDict(chaptersDict, textChapters);

    MetadataSet meta{}; // leave empty to reuse source ilst
    Mp4aConfig cfg{};
    cfg.sample_rate = aac.sample_rate;
    cfg.channel_count = aac.channel_config;
    cfg.sampling_index = aac.sampling_index;
    cfg.audio_object_type = aac.audio_object_type;

    bool ok = write_mp4(outputPath, aac, textChapters, imageChapters, cfg, meta,
                        /*fast_start=*/true, nullptr);
    if (!ok) {
        NSLog(@"Failed to write output");
    } else {
        NSLog(@"Wrote %@", [NSString stringWithUTF8String:outputPath.c_str()]);
    }
}
```

Adjust `duration_ms` usage if you represent start/duration differently. If you have parsed `ilst` metadata, pass it via the optional `ilst_payload` parameter on `write_mp4` to force reuse; otherwise, leaving `meta` empty will reuse the source ilst when available.
