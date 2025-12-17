//
//  meta_builder.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#pragma once
#include <memory>
#include <string>
#include <vector>

#include "metadata_set.hpp"
#include "mp4_atoms.hpp"

// ILST key/data builder.
std::unique_ptr<Atom> build_ilst_item(const std::string &fourcc, const std::vector<uint8_t> &data,
                                      uint32_t data_type = 1);

// ILST container builder.
std::unique_ptr<Atom> build_ilst(std::vector<std::unique_ptr<Atom>> items);

// Meta box builder.
std::unique_ptr<Atom> build_meta(std::unique_ptr<Atom> ilst);
// additional boxes such as `chpl` when constructing the final `udta`
std::unique_ptr<Atom> build_meta_atom(const MetadataSet &meta);

// Build a `meta` atom by reusing an existing `ilst` payload (as parsed from an.
// MP4/M4A). The payload must be the raw ilst payload (no atom header)
std::unique_ptr<Atom> build_meta_atom_from_ilst(const std::vector<uint8_t> &ilst_payload);
