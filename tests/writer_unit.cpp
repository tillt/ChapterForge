// Unit coverage for writer/builder helpers in mp4_muxer.cpp.
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include "aac_extractor.hpp"
#include "chapter_image_sample.hpp"
#include "chapter_text_sample.hpp"
#include "mp4a_builder.hpp"
#include "mp4_muxer.hpp"

using chapterforge::testing::build_audio_chunk_plan_for_test;
using chapterforge::testing::compute_durations_for_test;
using chapterforge::testing::derive_chunk_plan_for_test;
using chapterforge::testing::encode_tx3g_sample_for_test;
using chapterforge::testing::encode_tx3g_track_for_test;
using chapterforge::testing::TestDurationInfo;

namespace {

bool check(bool cond, const std::string &msg) {
    if (!cond) {
        fprintf(stderr, "[writer_unit] FAIL: %s\n", msg.c_str());
    }
    return cond;
}

std::vector<uint8_t> make_stsc_payload(const std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> &entries) {
    // version+flags (4 bytes) + entry_count (4 bytes) + N entries of 12 bytes.
    std::vector<uint8_t> out(8, 0);
    uint32_t ec = static_cast<uint32_t>(entries.size());
    out[4] = (ec >> 24) & 0xFF;
    out[5] = (ec >> 16) & 0xFF;
    out[6] = (ec >> 8) & 0xFF;
    out[7] = ec & 0xFF;
    for (const auto &e : entries) {
        uint32_t first_chunk, samples_per_chunk, sdi;
        std::tie(first_chunk, samples_per_chunk, sdi) = e;
        auto append_u32 = [&](uint32_t v) {
            out.push_back((v >> 24) & 0xFF);
            out.push_back((v >> 16) & 0xFF);
            out.push_back((v >> 8) & 0xFF);
            out.push_back(v & 0xFF);
        };
        append_u32(first_chunk);
        append_u32(samples_per_chunk);
        append_u32(sdi);
    }
    return out;
}

bool test_build_audio_chunk_plan() {
    bool ok = true;
    ok &= check(build_audio_chunk_plan_for_test(0).empty(), "empty plan for 0 samples");
    ok &= check(build_audio_chunk_plan_for_test(5) == std::vector<uint32_t>{5},
                "single chunk for small sample set");
    ok &= check(build_audio_chunk_plan_for_test(21) == std::vector<uint32_t>{21},
                "exact chunk size preserved");
    ok &= check(build_audio_chunk_plan_for_test(22) == std::vector<uint32_t>{21, 1},
                "tail chunk emitted");
    ok &= check(build_audio_chunk_plan_for_test(43) == std::vector<uint32_t>{21, 21, 1},
                "multiple full chunks plus tail");
    return ok;
}

bool test_derive_chunk_plan() {
    bool ok = true;
    // Single-entry table: chunk 1..N use 3 samples each.
    auto stsc_single = make_stsc_payload({{1, 3, 1}});
    ok &= check(derive_chunk_plan_for_test(stsc_single, 10) ==
                    std::vector<uint32_t>({3, 3, 3}),
                "derive_chunk_plan single entry (clamped to sample_count)");

    // Two entries: chunks 1-3 use 2 samples, from chunk 4 onward use 4.
    auto stsc_multi = make_stsc_payload({{1, 2, 1}, {4, 4, 1}});
    ok &= check(derive_chunk_plan_for_test(stsc_multi, 10) ==
                    std::vector<uint32_t>({2, 2, 2, 4}),
                "derive_chunk_plan multi entry");
    return ok;
}

bool test_encode_tx3g() {
    ChapterTextSample simple{};
    simple.text = "Hi";
    auto encoded = encode_tx3g_sample_for_test(simple);
    bool ok = check(encoded.size() == 2 + simple.text.size(), "len prefix written");
    ok &= check(encoded[0] == 0x00 && encoded[1] == 0x02, "text length big-endian");
    ok &= check(encoded[2] == 'H' && encoded[3] == 'i', "text bytes present");

    ChapterTextSample with_href{};
    with_href.text = "A";
    with_href.href = "http://x";
    auto enc_href = encode_tx3g_sample_for_test(with_href);
    std::string payload(enc_href.begin(), enc_href.end());
    ok &= check(payload.find("href") != std::string::npos, "href box present");
    ok &= check(payload.find("http://x") != std::string::npos, "href URL bytes present");
    return ok;
}

bool test_encode_track_counts() {
    ChapterTextSample a{.text = "One", .start_ms = 0};
    ChapterTextSample b{.text = "Two", .start_ms = 1000};
    auto samples = encode_tx3g_track_for_test({a, b});
    bool ok = check(samples.size() == 2, "two samples encoded");
    ok &= check(!samples[0].empty() && !samples[1].empty(), "samples non-empty");
    return ok;
}

bool test_compute_durations() {
    AacExtractResult aac{};
    aac.sample_rate = 1000;
    aac.frames.resize(10);  // audio_duration_ts = 10 * 1024 = 10240
    Mp4aConfig cfg{};

    ChapterTextSample t0{.text = "t0", .start_ms = 0};
    ChapterTextSample t1{.text = "t1", .start_ms = 5000};
    ChapterImageSample img0{};
    img0.start_ms = 0;

    TestDurationInfo info = compute_durations_for_test(aac, cfg, {t0, t1}, {img0});
    bool ok = true;
    ok &= check(info.audio_timescale == 1000, "audio_timescale propagated");
    ok &= check(info.audio_duration_ts == 10240, "audio_duration_ts computed");
    ok &= check(info.audio_duration_ms == 10240, "audio_duration_ms ms computed");
    ok &= check(info.text_ms == std::vector<uint32_t>({5000, 5240}),
                "text durations derived from starts");
    ok &= check(info.image_ms == std::vector<uint32_t>({10240}),
                "image duration fills remainder");
    return ok;
}

}  // namespace

int main() {
    bool ok = true;
    ok &= test_build_audio_chunk_plan();
    ok &= test_derive_chunk_plan();
    ok &= test_encode_tx3g();
    ok &= test_encode_track_counts();
    ok &= test_compute_durations();
    return ok ? 0 : 1;
}
