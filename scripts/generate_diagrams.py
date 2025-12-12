#!/usr/bin/env python3
"""
Generate simple PNG diagrams (no external deps) for ChapterForge docs.
Produces: docs/diagrams/chapter_tracks.png
"""

import os
import struct
import zlib

# Tiny 5x7 uppercase font (monospace). Keys are single characters.
FONT = {
    "A": [" 1 ", "1 1", "111", "1 1", "1 1"],
    "B": ["11 ", "1 1", "11 ", "1 1", "11 "],
    "C": [" 11", "1  ", "1  ", "1  ", " 11"],
    "D": ["11 ", "1 1", "1 1", "1 1", "11 "],
    "E": ["111", "1  ", "11 ", "1  ", "111"],
    "F": ["111", "1  ", "11 ", "1  ", "1  "],
    "G": [" 11", "1  ", "1  ", "1 1", " 11"],
    "H": ["1 1", "1 1", "111", "1 1", "1 1"],
    "I": ["111", " 1 ", " 1 ", " 1 ", "111"],
    "J": [" 11", "  1", "  1", "1 1", " 1 "],
    "K": ["1 1", "1 1", "11 ", "1 1", "1 1"],
    "L": ["1  ", "1  ", "1  ", "1  ", "111"],
    "M": ["1 1", "111", "1 1", "1 1", "1 1"],
    "N": ["1 1", "111", "111", "111", "1 1"],
    "O": ["111", "1 1", "1 1", "1 1", "111"],
    "P": ["111", "1 1", "111", "1  ", "1  "],
    "Q": ["111", "1 1", "1 1", "111", "  1"],
    "R": ["111", "1 1", "111", "11 ", "1 1"],
    "S": [" 11", "1  ", "111", "  1", "11 "],
    "T": ["111", " 1 ", " 1 ", " 1 ", " 1 "],
    "U": ["1 1", "1 1", "1 1", "1 1", "111"],
    "V": ["1 1", "1 1", "1 1", "1 1", " 1 "],
    "W": ["1 1", "1 1", "1 1", "111", "1 1"],
    "X": ["1 1", "1 1", " 1 ", "1 1", "1 1"],
    "Y": ["1 1", "1 1", " 1 ", " 1 ", " 1 "],
    "Z": ["111", "  1", " 1 ", "1  ", "111"],
    "0": ["111", "1 1", "1 1", "1 1", "111"],
    "1": [" 1 ", "11 ", " 1 ", " 1 ", "111"],
    "2": ["111", "  1", "111", "1  ", "111"],
    "3": ["111", "  1", "111", "  1", "111"],
    "4": ["1 1", "1 1", "111", "  1", "  1"],
    "5": ["111", "1  ", "111", "  1", "111"],
    "6": ["111", "1  ", "111", "1 1", "111"],
    "7": ["111", "  1", "  1", "  1", "  1"],
    "8": ["111", "1 1", "111", "1 1", "111"],
    "9": ["111", "1 1", "111", "  1", "111"],
    "-": ["   ", "   ", "111", "   ", "   "],
    ".": ["   ", "   ", "   ", "   ", " 1 "],
    "/": ["  1", "  1", " 1 ", "1  ", "1  "],
    ":": [" 1 ", "   ", "   ", "   ", " 1 "],
    "(": ["  1", " 1 ", " 1 ", " 1 ", "  1"],
    ")": ["1  ", " 1 ", " 1 ", " 1 ", "1  "],
    " ": ["   ", "   ", "   ", "   ", "   "],
}


def draw_text(img, x, y, text, color, scale=1):
    cursor = x
    for ch in text.upper():
        glyph = FONT.get(ch)
        if not glyph:
            cursor += 4 * scale
            continue
        for gy, row in enumerate(glyph):
            for gx, c in enumerate(row):
                if c == "1":
                    for sy in range(scale):
                        for sx in range(scale):
                            set_pixel(img, cursor + gx * scale + sx, y + gy * scale + sy, color)
        cursor += 4 * scale  # 3px glyph + 1px spacing


def set_pixel(img, x, y, color):
    if 0 <= x < img["w"] and 0 <= y < img["h"]:
        idx = (y * img["w"] + x) * 4
        img["data"][idx : idx + 4] = bytes(color)


def fill_rect(img, x0, y0, x1, y1, color):
    for y in range(y0, y1):
        for x in range(x0, x1):
            set_pixel(img, x, y, color)


def stroke_rect(img, x0, y0, x1, y1, color):
    for x in range(x0, x1):
        set_pixel(img, x, y0, color)
        set_pixel(img, x, y1 - 1, color)
    for y in range(y0, y1):
        set_pixel(img, x0, y, color)
        set_pixel(img, x1 - 1, y, color)


def make_image(w, h, bg=(0, 0, 0, 0)):
    return {"w": w, "h": h, "data": bytearray([bg[0], bg[1], bg[2], bg[3]] * w * h)}


def save_png(img, path):
    w, h = img["w"], img["h"]
    raw_rows = []
    for y in range(h):
        row = b"\x00" + bytes(img["data"][y * w * 4 : (y + 1) * w * 4])  # filter 0
        raw_rows.append(row)
    compressed = zlib.compress(b"".join(raw_rows), 9)

    def chunk(chunk_type, data):
        return (
            struct.pack(">I", len(data))
            + chunk_type
            + data
            + struct.pack(">I", zlib.crc32(chunk_type + data) & 0xFFFFFFFF)
        )

    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    png = [
        b"\x89PNG\r\n\x1a\n",
        chunk(b"IHDR", ihdr),
        chunk(b"IDAT", compressed),
        chunk(b"IEND", b""),
    ]
    with open(path, "wb") as f:
        for part in png:
            f.write(part)


def draw_chapter_tracks(path):
    img = make_image(1200, 620, bg=(20, 24, 28, 255))
    white = (240, 240, 240, 255)
    gray = (180, 180, 180, 255)
    blue = (84, 156, 255, 255)
    green = (120, 200, 120, 255)
    purple = (180, 140, 255, 255)
    orange = (255, 170, 90, 255)
    border = (80, 90, 100, 255)

    draw_text(img, 20, 20, "chapterforge m4a structure", white, scale=2)

    # moov block
    stroke_rect(img, 20, 90, 1000, 550, border)
    draw_text(img, 30, 100, "moov", gray, scale=2)

    # ftyp / mdat labels
    stroke_rect(img, 20, 50, 140, 80, border)
    draw_text(img, 30, 56, "ftyp", gray, scale=2)
    stroke_rect(img, 160, 50, 300, 80, border)
    draw_text(img, 170, 56, "free", gray, scale=2)
    stroke_rect(img, 320, 50, 1000, 80, border)
    draw_text(img, 330, 56, "mdat (audio + chapters)", gray, scale=2)
    draw_text(img, 700, 100, "fast-start: moov placed before mdat", gray, scale=2)

    # audio track
    stroke_rect(img, 60, 140, 460, 220, blue)
    draw_text(img, 70, 152, "trak audio (soun)", white, scale=2)
    draw_text(img, 70, 182, "long single audio track, reused stbl", gray, scale=2)

    # text titles
    stroke_rect(img, 60, 240, 460, 310, green)
    draw_text(img, 70, 252, "trak chapter titles (tx3g)", white, scale=2)
    draw_text(img, 70, 282, "padded samples + href=none", gray, scale=2)

    # text urls
    stroke_rect(img, 60, 330, 460, 400, purple)
    draw_text(img, 70, 342, "trak chapter urls (tx3g href)", white, scale=2)
    draw_text(img, 70, 372, "only if any chapter has url", gray, scale=2)

    # images
    stroke_rect(img, 60, 420, 460, 500, orange)
    draw_text(img, 70, 432, "trak chapter images (jpeg)", white, scale=2)

    # right side notes
    draw_text(img, 520, 230, "padding: chapter_count + 2 samples", gray, scale=2)
    draw_text(img, 520, 260, "href: start=0 end=0x000a length+url+pad", gray, scale=2)
    draw_text(img, 520, 300, "chapters: multiple text/url/image samples", gray, scale=2)
    draw_text(img, 520, 330, "handlers:", gray, scale=2)
    draw_text(img, 540, 360, "titles -> \"Chapter Titles\"", gray, scale=2)
    draw_text(img, 540, 390, "urls   -> \"Chapter URLs\"", gray, scale=2)
    draw_text(img, 540, 420, "images -> \"Chapter Images\"", gray, scale=2)

    # nesting indicators
    stroke_rect(img, 30, 120, 980, 520, border)

    save_png(img, path)


def main():
    out_dir = os.path.join(os.path.dirname(__file__), "..", "docs", "diagrams")
    os.makedirs(out_dir, exist_ok=True)
    draw_chapter_tracks(os.path.join(out_dir, "chapter_tracks.png"))
    print(f"Wrote {os.path.join(out_dir, 'chapter_tracks.png')}")


if __name__ == "__main__":
    main()
