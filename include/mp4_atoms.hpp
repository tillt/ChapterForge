//
//  mp4_atoms.hpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#pragma once
#include <cstdint>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "fourcc_utils.hpp"

// Forward declaration.
class Atom;

using AtomPtr = std::unique_ptr<Atom>;

class Atom {
   public:
    uint32_t type = 0;              // FourCC
    std::vector<uint8_t> payload;   // Raw payload (before children)
    std::vector<AtomPtr> children;  // Nested boxes

    uint32_t box_size = 0;  // Computed via fix_size_recursive()

    Atom() = default;
    explicit Atom(uint32_t t) : type(t) {}
    explicit Atom(const char t[4]) : type(fourcc(t)) {}

    // Factory.
    static AtomPtr create(const char t[4]);
    static AtomPtr create(uint32_t t);
    static AtomPtr create(const std::string &t);

    // Add child atom.
    void add(AtomPtr child);

    // Recursive search for atoms of given type.
    std::vector<Atom *> find(const std::string &t);

    // Recursive size computation.
    void fix_size_recursive();

    // Return size (must call fix_size_recursive first)
    uint32_t size() const;

    // Write atom to file.
    void write(std::ofstream &out) const;
};

// ------------- Helper write functions ---------------------------------------

inline void write_u8(std::vector<uint8_t> &p, uint8_t v) { p.push_back(v); }

inline void write_u16(std::vector<uint8_t> &p, uint16_t v) {
    p.push_back((v >> 8) & 0xFF);
    p.push_back(v & 0xFF);
}

inline void write_u24(std::vector<uint8_t> &p, uint32_t v) {
    p.push_back((v >> 16) & 0xFF);
    p.push_back((v >> 8) & 0xFF);
    p.push_back(v & 0xFF);
}

inline void write_u32(std::vector<uint8_t> &p, uint32_t v) {
    p.push_back((v >> 24) & 0xFF);
    p.push_back((v >> 16) & 0xFF);
    p.push_back((v >> 8) & 0xFF);
    p.push_back(v & 0xFF);
}

inline void write_u64(std::vector<uint8_t> &p, uint64_t v) {
    p.push_back((v >> 56) & 0xFF);
    p.push_back((v >> 48) & 0xFF);
    p.push_back((v >> 40) & 0xFF);
    p.push_back((v >> 32) & 0xFF);
    p.push_back((v >> 24) & 0xFF);
    p.push_back((v >> 16) & 0xFF);
    p.push_back((v >> 8) & 0xFF);
    p.push_back(v & 0xFF);
}

inline void write_fixed16_16(std::vector<uint8_t> &out, float f) {
    uint32_t v = static_cast<uint32_t>(f * 65536.0f);
    write_u32(out, v);
}
