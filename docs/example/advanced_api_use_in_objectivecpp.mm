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
        NSLog(@"Failed to write output: %@", [NSString stringWithUTF8String:outputPath.c_str()]);
    } else {
        NSLog(@"Wrote %@", [NSString stringWithUTF8String:outputPath.c_str()]);
    }
}
