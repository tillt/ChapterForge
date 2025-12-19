# ChapterForge API Documentation

Welcome to the C++ API reference for **ChapterForge**. This site is generated
from the public headers and is intended as a quick lookup for integrators that
embed ChapterForge in their own tools.

## What is ChapterForge?
- A library and CLI to mux chapter metadata (titles, optional URLs, optional
  images) into AAC/M4A files while preserving source metadata.
- Outputs Apple‑compatible chapter tracks (titles, URLs, thumbnails) that play
  correctly in QuickTime, Music.app, iTunes, VLC, and AVFoundation.

## Getting Started
- **Public API**: see the @ref api group for the main entry points and data
  structures.
- **CLI**: the same muxer functionality is available via the `chapterforge_cli`
  tool (see the README for usage examples).
- **Data model**: chapters are provided as @ref ChapterTextSample, optional URL
  samples (also @ref ChapterTextSample with `href`), and optional images as
  @ref ChapterImageSample. Top‑level metadata is described by @ref MetadataSet.

## Reference Layout
- @ref api : all public headers and functions.
- Internal helpers live in the source tree and are not part of the supported
  surface; only symbols listed in @ref api are considered stable.

## Further Reading
- The repository README contains end‑to‑end examples, JSON schema details, and
  player compatibility notes.
