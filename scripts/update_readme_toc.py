#!/usr/bin/env python3
"""
Update the README.md table of contents in place.

We scan all markdown headings (## and deeper) and regenerate the section between
the markers:
  <!-- TOC START (...) -->
  ...
  <!-- TOC END -->
"""

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
README = ROOT / "README.md"
START_RE = re.compile(r"<!-- TOC START")
END_RE = re.compile(r"<!-- TOC END -->")
HEADING_RE = re.compile(r"^(#{2,6})\s+(.*)$")


def slugify(text: str) -> str:
    # GitHub-style anchor slug
    slug = text.strip().lower()
    slug = re.sub(r"[^\w\s-]", "", slug)
    slug = re.sub(r"\s+", "-", slug)
    return slug


def parse_headings(lines):
    for line in lines:
        m = HEADING_RE.match(line)
        if not m:
            continue
        level = len(m.group(1))
        title = m.group(2).strip()
        yield level, title


def build_toc(lines):
    items = []
    for level, title in parse_headings(lines):
        # Skip the H1 title
        if level == 1:
            continue
        indent = "  " * (level - 2)
        anchor = slugify(title)
        items.append(f"{indent}- [{title}](#{anchor})")
    toc = ["## Table of Contents"] + items
    return "\n".join(toc) + "\n"


def main():
    if not README.exists():
        print("README.md not found", file=sys.stderr)
        return 1

    lines = README.read_text(encoding="utf-8").splitlines()
    toc = build_toc(lines)

    start_idx = None
    end_idx = None
    for i, line in enumerate(lines):
        if start_idx is None and START_RE.search(line):
            start_idx = i
        elif start_idx is not None and END_RE.search(line):
            end_idx = i
            break

    if start_idx is None or end_idx is None:
        print("TOC markers not found in README.md", file=sys.stderr)
        return 1

    new_lines = lines[: start_idx + 1]
    new_lines.extend(toc.splitlines())
    new_lines.extend(lines[end_idx:])

    new_content = "\n".join(new_lines) + "\n"
    README.write_text(new_content, encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
