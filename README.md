# ur-reader

A focused desktop manga reader for CBR/CBZ archives.

Most comic readers try to be too many things at once, or ship a pile of
toggles while getting the defaults and interactions wrong. ur-reader does one
thing well: **read a comic archive with the right combination of page
progression and view options.** Library/catalog management is deliberately
out of scope — the reader is a standalone tool that a separate library app can
drive (see [DESIGN.md](DESIGN.md)).

## Features

- **Formats** — CBZ (Zip) and CBR (RAR4/RAR5), via `libarchive`
- **Paged reading** — single page and double-page spreads, with automatic
  wide-spread detection and per-page overrides
- **Continuous scroll** — vertical webtoon-style reading
- **Reading direction** — RTL (manga) and LTR, with strict 2-zone navigation
- **Fit modes** — width, height, whole-page, original, and smart
- **Threaded decode** — background image decoding with a memory-budgeted
  LRU cache and prefetch
- **Resume support** — per-book progress (page, view settings, spread
  overrides) persisted to an atomic JSON file, keyed by a partial-hash book id
- **Scriptable** — CLI launch arguments in, progress file out; no library
  required

## Building

Requires a C++20 compiler, CMake ≥ 3.21, Qt 6, and libarchive.

```sh
# Debian/Ubuntu dependencies
sudo apt install qt6-base-dev libarchive-dev cmake g++

cmake -S . -B build
cmake --build build
```

This produces the `build/ur-reader` executable plus per-module smoke tests.

## Usage

```sh
ur-reader <archive> [--page N] [--mode paged|scroll]
                    [--direction rtl|ltr] [--session-id ID]
```

When an option is omitted, the reader falls back through: CLI argument →
saved progress → auto-detection → configured default.

### Controls

| Input | Action |
|---|---|
| Click left / right half | Previous / next (mapped to reading order — flips for RTL) |
| `Space` / `PageDown` | Next |
| `Backspace` / `PageUp` | Previous |
| `←` `→` | Page by physical direction |
| `Home` / `End` | First / last page |
| `M` / mode button | Toggle paged ↔ scroll |
| `F` | Toggle fullscreen (`Esc` or right-click to exit) |
| Scrubber | Jump to any page |

In fullscreen the chrome auto-hides and reveals on mouse movement.

## Project layout

The reader is built as seven modules in a one-way dependency chain —
`core → archive → model → decode → view`, with `session` and `app` on top.
The `archive` and `model` modules link no GUI code and are unit-testable in
isolation.

```
src/
  core/      shared types and utilities
  archive/   libarchive wrapper
  model/     page list, spread & reading-mode detection
  decode/    threaded decode + LRU cache
  view/      QGraphicsView reading widgets (paged + scroll)
  session/   book identity, progress file, global config
  app/       CLI, mode resolution, chrome, main window
tests/       one smoke test per module
```

Each module has a smoke test under `tests/`; the GUI tests run headless via
Qt's offscreen platform:

```sh
./build/archive_smoketest test_data/sample.cbz
./build/view_smoketest    test_data/sample.cbz
# ... and so on for model / cache / decode / session / app
```

## Design

See [DESIGN.md](DESIGN.md) for the full design rationale — the reader/library
decoupling contract, the progress-file schema, navigation and view-option
decisions, and known v1 simplifications.

## License

[MIT](LICENSE).
