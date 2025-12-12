//
//  trak_builder.cpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#include "trak_builder.hpp"

#include <cstdint>
#include <memory>
#include <vector>
#include <string>

#include "dinf_builder.hpp"
#include "hdlr_builder.hpp"
#include "mdhd_builder.hpp"
#include "mp4_atoms.hpp"
#include "nmhd_builder.hpp"
#include "smhd_builder.hpp"
#include "stbl_builder.hpp"  // for the stbl you pass in
#include "tkhd_builder.hpp"
#include "vmhd_builder.hpp"

// -----------------------------------------------------------------------------
// Helper: build tref/chap for audio track.
// -----------------------------------------------------------------------------
static std::unique_ptr<Atom> build_tref_chap(const std::vector<uint32_t> &refs) {
    auto tref = Atom::create("tref");
    auto chap = Atom::create("chap");
    auto &p = chap->payload;

    // referenced track IDs (big endian)
    for (uint32_t id : refs) {
        if (id != 0) {
            write_u32(p, id);
        }
    }

    tref->add(std::move(chap));
    return tref;
}

// -----------------------------------------------------------------------------
// Audio track.
// -----------------------------------------------------------------------------
std::unique_ptr<Atom> build_trak_audio(uint32_t track_id, uint32_t timescale, uint64_t duration_ts,
                                       std::unique_ptr<Atom> stbl_audio,
                                       const std::vector<uint32_t> &chapter_ref_track_ids,
                                       uint64_t tkhd_duration_mvhd) {
    auto trak = Atom::create("trak");

    // tkhd: audio (enabled, volume=1.0, width/height=0)
    trak->add(build_tkhd_audio(track_id, tkhd_duration_mvhd));

    // tref: chapter references.
    if (!chapter_ref_track_ids.empty()) {
        trak->add(build_tref_chap(chapter_ref_track_ids));
    }

    // mdia.
    auto mdia = Atom::create("mdia");
    mdia->add(build_mdhd(timescale, duration_ts, 0x55C4));  // mdhd (und)
    mdia->add(build_hdlr_sound());                          // hdlr (soun)

    // minf.
    auto minf = Atom::create("minf");
    minf->add(build_smhd());           // smhd
    minf->add(build_dinf());           // dinf
    minf->add(std::move(stbl_audio));  // stbl (audio sample table)

    mdia->add(std::move(minf));
    trak->add(std::move(mdia));

    return trak;
}

// -----------------------------------------------------------------------------
// Text chapter track.
// -----------------------------------------------------------------------------
std::unique_ptr<Atom> build_trak_text(uint32_t track_id, uint32_t timescale, uint64_t duration_ts,
                                      std::unique_ptr<Atom> stbl_text,
                                      uint64_t tkhd_duration_mvhd,
                                      const std::string &handler_name,
                                      bool enabled) {
    auto trak = Atom::create("trak");

    // tkhd: text track (disabled from normal playback, volume=0)
    trak->add(build_tkhd_text(track_id, tkhd_duration_mvhd, enabled));

    // mdia.
    auto mdia = Atom::create("mdia");
    mdia->add(build_mdhd(timescale, duration_ts, 0x15C7));  // mdhd, "eng"
    mdia->add(build_hdlr_text(handler_name));               // hdlr: custom

    // minf.
    auto minf = Atom::create("minf");
    minf->add(build_nmhd());          // nmhd for text
    minf->add(build_dinf());          // dinf
    minf->add(std::move(stbl_text));  // stbl for tx3g

    mdia->add(std::move(minf));
    trak->add(std::move(mdia));

    return trak;
}

// -----------------------------------------------------------------------------
// Timed metadata track (metadata samples aligned to chapters).
// -----------------------------------------------------------------------------
std::unique_ptr<Atom> build_trak_metadata(uint32_t track_id, uint32_t timescale,
                                          uint64_t duration_ts,
                                          std::unique_ptr<Atom> stbl_metadata,
                                          uint64_t tkhd_duration_mvhd,
                                          const std::string &handler_name) {
    auto trak = Atom::create("trak");

    // tkhd: metadata track (disabled from playback, no dimensions)
    trak->add(build_tkhd_text(track_id, tkhd_duration_mvhd, true));

    auto mdia = Atom::create("mdia");
    mdia->add(build_mdhd(timescale, duration_ts, 0x15C7));      // mdhd, "eng"
    mdia->add(build_hdlr_metadata(handler_name));               // hdlr: meta

    auto minf = Atom::create("minf");
    minf->add(build_nmhd());                                    // nmhd
    minf->add(build_dinf());                                    // dinf
    minf->add(std::move(stbl_metadata));                        // stbl

    mdia->add(std::move(minf));
    trak->add(std::move(mdia));

    return trak;
}

// -----------------------------------------------------------------------------
// Image chapter track.
// -----------------------------------------------------------------------------
std::unique_ptr<Atom> build_trak_image(uint32_t track_id, uint32_t timescale, uint64_t duration_ts,
                                       std::unique_ptr<Atom> stbl_image, uint16_t width,
                                       uint16_t height, uint64_t tkhd_duration_mvhd) {
    auto trak = Atom::create("trak");

    // tkhd: image/video track (disabled from normal playback, volume=0)
    trak->add(build_tkhd_image(track_id, tkhd_duration_mvhd, width, height));

    // edts/elst to map full movie timeline to media time 0.
    {
        auto edts = Atom::create("edts");
        auto elst = Atom::create("elst");
        auto &p = elst->payload;
        write_u8(p, 0);
        write_u24(p, 0);  // version/flags
        write_u32(p, 1);  // entry count
        write_u32(p,
                  static_cast<uint32_t>(tkhd_duration_mvhd));  // segment duration (movie timescale)
        write_u32(p, 0);                                       // media_time
        write_u32(p, 0x00010000);                              // media_rate (1.0)
        edts->add(std::move(elst));
        trak->add(std::move(edts));
    }

    // mdia.
    auto mdia = Atom::create("mdia");
    mdia->add(build_mdhd(timescale, duration_ts, 0x15C7));  // mdhd, "eng"
    mdia->add(build_hdlr_video());                          // hdlr: "Chapter Images"

    // minf.
    auto minf = Atom::create("minf");
    minf->add(build_vmhd());           // vmhd
    minf->add(build_dinf());           // dinf
    minf->add(std::move(stbl_image));  // stbl for JPEG

    mdia->add(std::move(minf));
    trak->add(std::move(mdia));

    return trak;
}
