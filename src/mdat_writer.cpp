//
//  mdat_writer.cpp.
//  ChapterForge.
//
//  Created by Till Toenshoff on 12/9/25.
//

#include "mdat_writer.hpp"

#include <stdexcept>

// -----------------------------------------------------------------------------
// Write mdat and record sample offsets.
// -----------------------------------------------------------------------------
MdatOffsets write_mdat(std::ofstream &out, const std::vector<std::vector<uint8_t>> &audio_samples,
                       const std::vector<std::vector<uint8_t>> &text_samples,
                       const std::vector<std::vector<uint8_t>> &image_samples,
                       const std::vector<uint32_t> &audio_chunk_sizes,
                       const std::vector<uint32_t> &text_chunk_sizes,
                       const std::vector<uint32_t> &image_chunk_sizes) {
    MdatOffsets result;

    // Start of mdat box.
    uint64_t mdat_header_pos = out.tellp();

    // Size placeholder (4 bytes) + 'mdat'
    uint8_t header[8] = {0, 0, 0, 0, 'm', 'd', 'a', 't'};
    out.write(reinterpret_cast<char *>(header), 8);

    // Payload begins right after 'mdat'
    uint64_t payload_start = out.tellp();
    result.payload_start = payload_start;

    auto write_track = [&](const std::vector<std::vector<uint8_t>> &samples,
                           const std::vector<uint32_t> &chunk_sizes,
                           std::vector<uint32_t> &offsets) {
        if (samples.empty()) {
            return;
        }

        // default: one sample per chunk when no plan is provided.
        std::vector<uint32_t> plan =
            chunk_sizes.empty() ? std::vector<uint32_t>(samples.size(), 1) : chunk_sizes;

        size_t sample_index = 0;
        for (uint32_t chunk_size : plan) {
            if (sample_index >= samples.size()) {
                break;
            }
            uint64_t pos = out.tellp();
            uint32_t rel = static_cast<uint32_t>(pos - payload_start);
            offsets.push_back(rel);

            for (uint32_t i = 0; i < chunk_size && sample_index < samples.size(); ++i) {
                const auto &sample = samples[sample_index++];
                out.write(reinterpret_cast<const char *>(sample.data()), sample.size());
            }
        }

        // Write any stragglers if plan was shorter than sample count.
        if (sample_index < samples.size()) {
            uint64_t pos = out.tellp();
            uint32_t rel = static_cast<uint32_t>(pos - payload_start);
            offsets.push_back(rel);
            for (; sample_index < samples.size(); ++sample_index) {
                const auto &sample = samples[sample_index];
                out.write(reinterpret_cast<const char *>(sample.data()), sample.size());
            }
        }
    };

    // Apple convention: audio first, then text, then image.
    write_track(audio_samples, audio_chunk_sizes, result.audio_offsets);
    write_track(text_samples, text_chunk_sizes, result.text_offsets);
    write_track(image_samples, image_chunk_sizes, result.image_offsets);

    // Patch mdat size field.
    uint64_t end_pos = out.tellp();
    uint64_t box_size = end_pos - mdat_header_pos;

    if (box_size > 0xFFFFFFFFULL) {
        throw std::runtime_error("mdat too large ( > 4 GB )");
    }

    uint32_t size32 = static_cast<uint32_t>(box_size);
    uint8_t size_bytes[4] = {
        static_cast<uint8_t>((size32 >> 24) & 0xFF), static_cast<uint8_t>((size32 >> 16) & 0xFF),
        static_cast<uint8_t>((size32 >> 8) & 0xFF), static_cast<uint8_t>((size32) & 0xFF)};

    out.seekp(mdat_header_pos);
    out.write(reinterpret_cast<char *>(size_bytes), 4);
    out.seekp(end_pos);

    return result;
}

// -----------------------------------------------------------------------------
// Patch a single stco table.
// -----------------------------------------------------------------------------
void patch_stco_table(Atom *stco, const std::vector<uint32_t> &offsets,
                      uint64_t mdat_payload_start) {
    if (!stco) {
        return;
    }
    auto &p = stco->payload;

    if (p.size() < 8) {
        return;
    }

    uint32_t entry_count = (p[4] << 24) | (p[5] << 16) | (p[6] << 8) | (p[7]);

    entry_count = std::min<uint32_t>(entry_count, offsets.size());

    size_t pos = 8;
    for (uint32_t i = 0; i < entry_count; ++i) {
        uint32_t abs_offset = offsets[i] + static_cast<uint32_t>(mdat_payload_start);

        p[pos + 0] = (abs_offset >> 24) & 0xFF;
        p[pos + 1] = (abs_offset >> 16) & 0xFF;
        p[pos + 2] = (abs_offset >> 8) & 0xFF;
        p[pos + 3] = (abs_offset) & 0xFF;

        pos += 4;
    }
}

// -----------------------------------------------------------------------------
// Patch all stco atoms (in order: audio, text, image)
// -----------------------------------------------------------------------------
void patch_all_stco(Atom *moov, const MdatOffsets &offs, bool patch_audio) {
    if (!moov) {
        return;
    }

    auto stcos = moov->find("stco");

    // Order is deterministic from our builder: audio, text, image.
    if (patch_audio && stcos.size() >= 1) {
        patch_stco_table(stcos[0], offs.audio_offsets, offs.payload_start);
    }
    if (stcos.size() >= 2) {
        patch_stco_table(stcos[1], offs.text_offsets, offs.payload_start);
    }
    if (stcos.size() >= 3) {
        patch_stco_table(stcos[2], offs.image_offsets, offs.payload_start);
    }
}

// -----------------------------------------------------------------------------
// Compute offsets only (no writing), given a payload start.
// -----------------------------------------------------------------------------
MdatOffsets compute_mdat_offsets(uint64_t payload_start,
                                 const std::vector<std::vector<uint8_t>> &audio_samples,
                                 const std::vector<std::vector<uint8_t>> &text_samples,
                                 const std::vector<std::vector<uint8_t>> &image_samples,
                                 const std::vector<uint32_t> &audio_chunk_sizes,
                                 const std::vector<uint32_t> &text_chunk_sizes,
                                 const std::vector<uint32_t> &image_chunk_sizes) {
    MdatOffsets result;
    result.payload_start = payload_start;
    uint64_t cursor = payload_start;

    auto compute_track = [&](const std::vector<std::vector<uint8_t>> &samples,
                             const std::vector<uint32_t> &chunk_sizes,
                             std::vector<uint32_t> &offsets) {
        if (samples.empty()) {
            return;
        }

        std::vector<uint32_t> plan =
            chunk_sizes.empty() ? std::vector<uint32_t>(samples.size(), 1) : chunk_sizes;

        size_t sample_index = 0;
        for (uint32_t chunk_size : plan) {
            if (sample_index >= samples.size()) {
                break;
            }
            offsets.push_back(static_cast<uint32_t>(cursor - payload_start));
            for (uint32_t i = 0; i < chunk_size && sample_index < samples.size();
                 ++i, ++sample_index) {
                cursor += samples[sample_index].size();
            }
        }
        if (sample_index < samples.size()) {
            offsets.push_back(static_cast<uint32_t>(cursor - payload_start));
            for (; sample_index < samples.size(); ++sample_index) {
                cursor += samples[sample_index].size();
            }
        }
    };

    // order: audio, text, image.
    compute_track(audio_samples, audio_chunk_sizes, result.audio_offsets);
    compute_track(text_samples, text_chunk_sizes, result.text_offsets);
    compute_track(image_samples, image_chunk_sizes, result.image_offsets);

    return result;
}
