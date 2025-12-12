//
//  mp4_atoms.cpp
//  ChapterForge
//
//  Created by Till Toenshoff on 12/9/25.
//  Copyright © 2025 Till Toenshoff. All rights reserved.
//

#include "mp4_atoms.hpp"

// -----------------------------------------------------------------------------
// Factory.
// -----------------------------------------------------------------------------
AtomPtr Atom::create(const char t[4]) { return std::make_unique<Atom>(t); }

AtomPtr Atom::create(uint32_t t) { return std::make_unique<Atom>(t); }

AtomPtr Atom::create(const std::string &t) { return std::make_unique<Atom>(fourcc(t)); }

// -----------------------------------------------------------------------------
// Add child.
// -----------------------------------------------------------------------------
void Atom::add(AtomPtr child) { children.push_back(std::move(child)); }

// -----------------------------------------------------------------------------
// Recursive find.
// -----------------------------------------------------------------------------
std::vector<Atom *> Atom::find(const std::string &t) {
    std::vector<Atom *> result;

    uint32_t want = fourcc(t);

    if (type == want) {
        result.push_back(this);
    }

    for (auto &child : children) {
        auto sub = child->find(t);
        result.insert(result.end(), sub.begin(), sub.end());
    }

    return result;
}

// -----------------------------------------------------------------------------
// Compute recursive box size.
// -----------------------------------------------------------------------------
void Atom::fix_size_recursive() {
    // Start with MP4 header: 8 bytes (size + type)
    uint32_t total = 8;

    // Payload.
    total += payload.size();

    // Children.
    for (auto &c : children) {
        c->fix_size_recursive();
        total += c->box_size;
    }

    box_size = total;
}

// -----------------------------------------------------------------------------
// Return box size.
// -----------------------------------------------------------------------------
uint32_t Atom::size() const { return box_size; }

// -----------------------------------------------------------------------------
// Write atom to file.
// -----------------------------------------------------------------------------
void Atom::write(std::ofstream &out) const {
    uint32_t s = box_size;

    uint8_t header[8];
    header[0] = (s >> 24) & 0xFF;
    header[1] = (s >> 16) & 0xFF;
    header[2] = (s >> 8) & 0xFF;
    header[3] = (s) & 0xFF;

    header[4] = (type >> 24) & 0xFF;
    header[5] = (type >> 16) & 0xFF;
    header[6] = (type >> 8) & 0xFF;
    header[7] = (type) & 0xFF;

    out.write(reinterpret_cast<const char *>(header), 8);

    // Write payload.
    if (!payload.empty()) {
        out.write(reinterpret_cast<const char *>(payload.data()), payload.size());
    }

    // Write children.
    for (const auto &c : children) {
        c->write(out);
    }
}
