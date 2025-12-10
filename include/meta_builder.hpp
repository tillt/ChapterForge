//
//  meta_builder.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//

#pragma once
#include "mp4_atoms.hpp"
#include <memory>
#include <string>
#include <vector>

// ILST key/data builder
std::unique_ptr<Atom> build_ilst_item(const std::string &fourcc,
                                      const std::vector<uint8_t> &data,
                                      uint32_t data_type = 1);

// ILST container builder
std::unique_ptr<Atom> build_ilst(std::vector<std::unique_ptr<Atom>> items);

// Meta box builder
std::unique_ptr<Atom> build_meta(std::unique_ptr<Atom> ilst);

struct MetadataSet {
  std::string title;
  std::string artist;
  std::string album;
  std::string genre;
  std::string year;
  std::string comment;

  // Cover art (JPEG or PNG), raw bytes
  std::vector<uint8_t> cover;
};

// Build the `meta` atom (without wrapping it in `udta`). The caller can add
// additional boxes such as `chpl` when constructing the final `udta`.
std::unique_ptr<Atom> build_meta_atom(const MetadataSet &meta);

// Build a `meta` atom by reusing an existing `ilst` payload (as parsed from an
// MP4/M4A). The payload must be the raw ilst payload (no atom header).
std::unique_ptr<Atom> build_meta_atom_from_ilst(
    const std::vector<uint8_t> &ilst_payload);
