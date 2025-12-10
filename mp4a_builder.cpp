//
//  mp4a_builder.cpp
//  mp4chapters
//
//  Created by Till Toenshoff on 12/9/25.
//

#include "mp4a_builder.hpp"
#include <algorithm>

// helper: write MPEG-4 descriptor length (7-bit, continuation)
static void write_descr_len(std::vector<uint8_t> &out, uint32_t len) {
  uint8_t bytes[4];
  int count = 0;
  do {
    bytes[count++] = len & 0x7F;
    len >>= 7;
  } while (len > 0 && count < 4);

  for (int i = count - 1; i >= 0; --i) {
    uint8_t b = bytes[i];
    if (i != 0)
      b |= 0x80; // set continuation bit
    out.push_back(b);
  }
}

// helper: write descriptor length using 4-byte continuation form
static void write_descr_len_padded4(std::vector<uint8_t> &out, uint8_t len) {
  // 4-byte form: 0x80 0x80 0x80 len (matches Apple/iTunes style)
  out.push_back(0x80);
  out.push_back(0x80);
  out.push_back(0x80);
  out.push_back(len);
}

// -----------------------------------------------------------------------------
// Build DecoderSpecificInfo (AudioSpecificConfig)
// ISO/IEC 14496-3 Table 1.13
// -----------------------------------------------------------------------------
static std::vector<uint8_t>
build_audio_specific_config(uint8_t audio_object_type,
                            uint8_t sampling_freq_index,
                            uint8_t channel_config) {
  std::vector<uint8_t> asc;

  // 5 bits: audioObjectType
  // 4 bits: samplingFrequencyIndex
  // 4 bits: channelConfiguration
  // 3 bits unused (set to 0)
  uint8_t byte1 =
      ((audio_object_type & 0x1F) << 3) | ((sampling_freq_index & 0x0E) >> 1);

  uint8_t byte2 =
      ((sampling_freq_index & 0x01) << 7) | ((channel_config & 0x0F) << 3);

  asc.push_back(byte1);
  asc.push_back(byte2);

  return asc;
}

// -----------------------------------------------------------------------------
// Build ESDS box
// -----------------------------------------------------------------------------
static std::unique_ptr<Atom> build_esds(const Mp4aConfig &cfg) {
  auto esds = Atom::create("esds");
  auto &p = esds->payload;

  // version + flags
  write_u8(p, 0);
  write_u24(p, 0);

  auto asc = build_audio_specific_config(
      cfg.audio_object_type, cfg.sampling_index, cfg.channel_config);

  // Build ES_Descriptor (size = 0x22) using 4-byte length encoding to mirror
  // QT/iTunes
  write_u8(p, 0x03);
  write_descr_len_padded4(p, 0x22);
  write_u16(p, 0x0000); // ES_ID
  write_u8(p, 0x00);    // flags

  // DecoderConfigDescriptor (size = 0x14)
  write_u8(p, 0x04);
  write_descr_len_padded4(p, 0x14);
  write_u8(p, 0x40);        // objectTypeIndication: MPEG-4 Audio
  write_u8(p, 0x15);        // streamType=audio, upStream=0, reserved=1
  write_u24(p, 0x00018300); // bufferSizeDB
  write_u32(p, 0x0147F000); // maxBitrate
  write_u32(p, 0x01388105); // avgBitrate

  // DecoderSpecificInfo (ASC)
  write_u8(p, 0x05);
  write_descr_len_padded4(
      p, static_cast<uint8_t>(asc.size())); // typically 2 bytes
  p.insert(p.end(), asc.begin(), asc.end());

  // SLConfigDescriptor
  write_u8(p, 0x06);
  write_descr_len_padded4(p, 0x01);
  write_u8(p, 0x02);

  return esds;
}

// -----------------------------------------------------------------------------
// Build mp4a sample entry
// -----------------------------------------------------------------------------
std::unique_ptr<Atom> build_mp4a(const Mp4aConfig &cfg) {
  auto mp4a = Atom::create("mp4a");
  auto &p = mp4a->payload;

  // SampleEntry header: 6 bytes reserved, then data_reference_index
  write_u32(p, 0); // 4 reserved bytes
  write_u16(p, 0); // 2 reserved bytes
  write_u16(p, 1); // data_reference_index = 1

  // AudioSampleEntry fields (reserved[2])
  write_u32(p, 0);
  write_u32(p, 0);

  write_u16(p, cfg.channel_count);
  write_u16(p, cfg.sample_size);

  write_u16(p, 0); // pre_defined
  write_u16(p, 0); // reserved

  // 32-bit fixed-point sample rate (sample_rate << 16)
  write_u32(p, cfg.sample_rate << 16);

  // add ESDS atom
  mp4a->add(build_esds(cfg));

  return mp4a;
}
