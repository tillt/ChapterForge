# ChapterForge

ChapterForge is a library and CLI to mux chapters (text and optional images) into AAC/M4A files while preserving metadata and handling Apple-compatible chapter tracks.

## Motivation and Backstory

The MPEG4 standard does not explicitly describe chapter marks - they do however exist. Back in the days, under the umbrella of the QuickTime.framework, Apple had released authoring tools for audiobooks. With those tools you could add jump-marks to an M4A file. Those jump-marks could have a description text, an image and possibly more. That way a user could conveniently jump to specific sections of the audio -- useful for example as a mark for chapters. There are many players that understand those, but not all do. Specifically Apple who has pushed for this "extension" of the standard, has traditionally been understanding it in their players. That is true for Music.app, iTunes.app, QuickTime.app and even Books.app. All of them today support chapter marks. Windows has existing support there as well. That said it becomes clear, this is not a totally accepted standard but at least a functional solution for the challenge of encoding such thing into the audio file.
As already hinted, Apple did support authoring tools - but that was way back in PowerPC times. The QTKit is long gone. There appears to be not a single open source tool in the market that would support a recent OS. There certainly are tools like ffmpeg - it does even support chapter marks for MP4 - but - no images for those. The only tool in the market supporting images in chapter marks in 2025 appears to be Auphonic - commercial. All the existing libraries, even the commercial ones like Bento4 do not support chapter marks with images.
This gets even more arcane, from my perspective. AVFoundation, the framework Apple offers these days for media playback and authoring does support reading of MP4 chapter marks. Thus creating a player supporting that feature is trivial. The kicker here is, Apple does not support any way of writing such files - none at all.

The situation is rather bizarre and no one has a strong enough interest to change this, until today. The player I am tinkering with needs support for storing a track-list / set-list in the file itself. That way I can attribute those beautiful DJ sets and have neat track-mark thumbnails and descriptions on the player, persisted in the M4A file.


## Features

ChapterForge uses the audio track from the input. It then combines that with a text track for the description and a video track for the optional chapter images. All of that information gets bundled in the resulting output M4A file. With that M4A file you can now see chapter marks in your player.


## Platforms

Supported players (just a selection of known goods):

|               | text  | image |
| :---:         | :---: | :---: |
| QuickTime.app | X     | X     |
| Music.app     | X     | X     |
| Books.app     | X     | X     |
| VLC           | X     |       |


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
struct ChapterTextSample {
  std::string text;       // UTF-8 chapter title
  uint32_t start_ms = 0;  // absolute start time in milliseconds
};

struct ChapterImageSample {
  std::vector<uint8_t> data; // JPEG bytes for this chapter frame
  uint32_t start_ms = 0;     // absolute start time in milliseconds
};

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

## Atom flow (input → output)

ChapterForge preserves the source audio track and metadata (`ilst`) and adds two new tracks for chapters:

```
Input (AAC in M4A/MP4)
├─ ftyp
├─ free (optional)
├─ moov
│  ├─ mvhd
│  ├─ trak (audio)
│  │  ├─ tkhd
│  │  └─ mdia → minf → stbl (reused, including stsd/stts/stsc/stsz/stco)
│  └─ udta/meta/ilst (reused if present)
└─ mdat

Output (ChapterForge)
├─ ftyp
├─ free (optional or moved for faststart)
├─ moov
│  ├─ mvhd
│  ├─ trak (audio, reused stbl when input is MP4/M4A)
│  ├─ trak (chapter text, tx3g)
│  │  └─ stbl with stsd(tx3g) + stts/stsc/stsz/stco
│  ├─ trak (chapter images, jpeg)
│  │  └─ stbl with stsd(jpeg) + stts/stsc/stsz/stco/stss
│  └─ udta/meta/ilst (reused if present, otherwise from JSON)
└─ mdat (audio + chapter samples)
```

Fast-start repacks `moov` ahead of `mdat` when requested.

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
        NSNumber *startMs = entry[@"time"];
        if (!title || !startMs) continue;
        ChapterTextSample s{};
        s.text = [title UTF8String];
        s.start_ms = [startMs unsignedIntValue];
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

If you have parsed `ilst` metadata, pass it via the optional `ilst_payload` parameter on `write_mp4` to force reuse; otherwise, leaving `meta` empty will reuse the source ilst when available.
