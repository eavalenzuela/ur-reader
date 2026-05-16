# ur-reader — Design Document

A focused desktop manga reader for CBR/CBZ (and CB7/CBT) archives.

## Philosophy

Most manga readers try to be too many things at once, or ship a pile of
toggles while getting the *defaults and interactions* wrong. ur-reader does
one thing well: **read a comic archive with the right combination of page
progression and view options.**

The library/catalog concern is deliberately **out of scope** for the reader
itself. The reader is a standalone executable. A separate library app (built
later, independently) launches the reader and consumes the progress it writes.

## Scope

**In scope (v1)**
- Open CBZ (Zip) and CBR (RAR4 / RAR5) archives.
- Paged reading: single page and double-page spread.
- Continuous vertical scroll (webtoon) reading.
- RTL and LTR reading direction.
- Auto-detection of reading mode; CLI override; in-app toggle.
- Spread (double-page art) detection with manual per-page override.
- Essential + high-value view options (see below).
- Resume support via a per-book progress file.
- CLI args so an external library app can drive it.

**Out of scope (v1)**
- Library / catalog / metadata browser (separate app).
- CB7 (7z) and CBT (Tar) — deferred post-v1; `libarchive` handles them via
  the same API, so adding them is just an extension allow-list change.
- Parsing / acting on `ComicInfo.xml` — see "ComicInfo.xml" below.
- Page rotation, color adjustment, configurable downscale filter.
- Network features, online sources, downloading.

---

## Architecture

### Decoupling: reader vs library

The reader is one process and **never knows the library exists**. The contract
between them is two one-directional channels:

1. **Library → Reader**: command-line arguments at launch.
2. **Reader → Library**: a per-book *progress file* the reader writes
   continuously (debounced). The library reads it whenever it wants.

Ownership rule:

- The **reader** owns *"where am I in this book, and how am I viewing it."*
- The **library** owns *"what books exist and how they relate."*

No overlap. Because the reader writes its own progress file, it also has
resume support when run completely standalone — the library is just an
optional consumer of the same files.

### Modules

Each module is independently testable. The archive layer and book model do
not link Qt GUI.

1. **Archive layer** — open archive, list entries, extract entry to memory by
   index. Pure C++, no Qt GUI. Backed by `libarchive` (one read-only
   dependency; v1 uses Zip + RAR, the same API later covers 7z/Tar). Detects
   and stashes `ComicInfo.xml` without parsing it.
2. **Book model** — natural-sorted ordered page list, aspect-ratio cache,
   spread flags, metadata. No Qt GUI.
3. **Decode/cache service** — async image decode on a thread pool; prefetch
   ±N pages; explicit LRU bitmap cache with a memory budget.
4. **View widgets** — `PagedView` and `ScrollView`, both fed by the same Book
   model.
5. **Session/progress** — load and save the per-book progress file.
6. **App shell** — argument parsing, mode resolution, window chrome, the
   mode toggle, keybindings, fullscreen handling.

### Technology

- **Language/UI**: C++ / Qt.
- **Archives**: `libarchive` — single dependency. v1 reads cbz (Zip) and cbr
  (RAR4/RAR5; RAR5 read support since libarchive 3.2.0). The same API covers
  cb7/cbt when those are enabled post-v1.
- **Paged rendering**: `QGraphicsView` + `QGraphicsScene` — smooth zoom/pan,
  fit modes, and double-page layout (two `QGraphicsPixmapItem`s).
- **Scroll rendering**: QML `ListView`/`Flickable` with delegate recycling, or
  a tall `QGraphicsScene`. Recycling preferred for very long webtoon chapters.
- **Decode pipeline**: `QImageReader` on a `QThreadPool`; custom LRU cache
  (not `QPixmapCache` — we want explicit memory control).
- **Page ordering**: natural sort of archive entry names. Never trust the
  archive's stored order.
- **RTL**: handled in *our* page-index logic and click-zone logic. Qt's
  `layoutDirection` is used only for chrome widgets, never for page flow.

---

## Reading modes

Two first-class modes: **paged** and **scroll**. Mode is resolved by this
precedence:

```
1. CLI --mode argument            (highest priority)
2. saved progress file (per-book remembered mode)
3. auto-detection
4. global default from config     (fallback)
```

The in-app **top-right toggle** overrides the current session and writes the
new mode into the progress file, so it becomes the remembered default. The
toggle is just "user-supplied detection result" — one code path.

### Auto-detection

Sample ~5–10 pages' aspect ratios:

- Most pages very tall (height/width ≳ 2.5), often uniform width → **scroll**.
- Normal page ratio (height/width ≈ 1.4) → **paged**.

### Spread detection (separate from mode)

Within a paged book, any landscape page (width > height) is flagged as a
standalone double-page spread, so double-page mode does not mispair it with a
normal portrait page. The user can override per page (force-single /
force-pair); overrides persist in the progress file.

---

## Navigation

### Paged mode

- **Click zones**: strict 2-zone — left half / right half. Every pixel pages.
  Zones map to **reading order**, not screen sides: in RTL the left zone is
  "next." Zones flip automatically with reading direction, including when the
  user toggles direction mid-book.
- **Keyboard**:
  - `Space` / `PageDown` = **next**, always (reading-order, never flips).
  - `←` / `→` = **physical** direction (left arrow always moves left visually;
    in RTL that means `←` = next).
  - `Home` / `End` = first / last page.
  - `G` = go to page.
  - `F` = toggle fullscreen. `Esc` = exit fullscreen.
  - `D` = toggle double-page. `M` = toggle reading mode.
- **Scrubber**: a page slider in the chrome; runs right-to-left in RTL so
  dragging matches page flow. Thumbnail preview on hover/drag.
- **End-of-book**: "next" past the last page shows a subtle end-of-book
  affordance rather than silently doing nothing. (Future hook: the library can
  use this for "next chapter.")

### Scroll mode

Kept consistent with paged — same mental model, no third zone anywhere.

- Wheel / trackpad / drag = continuous scroll (primary input).
- Left/right click zones page up/down (~90% of viewport, with overlap).
- `Space` = jump ~90% of viewport height.
- **Free-scroll by default**; per-book toggle to **snap** to page boundaries.

---

## Chrome (menus & navigation UI)

- **Windowed mode**: chrome (scrubber, view-option controls, mode toggle) is
  **normally visible** — persistent, not auto-hiding.
- **Fullscreen mode**: chrome **auto-hides**. It auto-reveals on mouse
  movement and auto-hides again after ~2s idle.
- **Exiting fullscreen**: `Esc` key or the **right-click context menu**.
- Right-click anywhere opens a context menu (view options, exit fullscreen,
  go-to-page). Right-click is otherwise unused, so it is free real estate.
- When chrome is visible, clicks on chrome widgets do not fall through to
  paging — only clicks on the bare page area page.

---

## View options

Scope for v1: **essential + high-value**. Rotation, filter quality, and color
adjustment are deferred.

| Option                              | Scope        | Notes                                         |
|-------------------------------------|--------------|-----------------------------------------------|
| Fit: width / height / whole / 1:1 / smart | per-book | smart = whole-page, but fit-width for tall pages |
| Background color                    | global       | black / dark gray / white / custom; default dark gray |
| Double-page on/off                  | per-book     | with cover-alone offset (first page unpaired) |
| Pair-offset nudge                   | per-book     | shift pairing to fix misaligned spreads       |
| Spread override (force single/pair) | per-book, per-page | persisted in progress file              |
| Zoom & pan                          | per-session  | resets on book open                           |
| Auto-trim borders                   | per-book     | crop uniform whitespace from badly scanned pages |
| Page gap                            | global       | spacing in spreads and in scroll              |

### Persistence split

- **Global config file**: background color, page gap, global default mode,
  prefetch/cache budget.
- **Per-book progress file**: current page, reading mode, reading direction,
  fit mode, double-page on/off, pair-offset, per-page spread overrides,
  auto-trim on/off, snap on/off.
- **Not persisted**: zoom/pan (per-session; reset on book open).

---

## Reader ↔ Library contract

### Launch (Library → Reader)

```
ur-reader <archive-path> [--page N] [--mode paged|scroll]
          [--direction rtl|ltr] [--session-id <uuid>]
```

- `<archive-path>` — required.
- `--page` / `--mode` / `--direction` — optional overrides. When absent, the
  reader falls back through the resolution precedence above.
- `--session-id` — optional correlation id the library can pass to match a
  launch with its progress file.

### Progress file (Reader → Library)

- One file per book, in `QStandardPaths::AppDataLocation`
  (`~/.local/share/ur-reader/progress/<id>.json` on Linux). Glob-able by the
  library, individually deletable.
- Written by the reader, debounced (e.g. on page change, settle ~1s).
- **Atomic writes**: write to a temp file, then `rename()` over the real one,
  so the library never reads a half-written file.
- The reader is the **sole writer**; the library is **strictly read-only** —
  no merge problem.
- Standalone reader uses the same file for its own resume support.
- The file is pure persistent state — it carries **no live "currently
  reading" indicator** and nothing about whether a reader process is running.

#### Book identity

`book.id` is the file key. It is computed as:

```
"p1:" + hex( SHA-256( file_size_bytes ++ first_64KB ++ last_64KB ) )
```

The `p1:` prefix tags the partial-hash *algorithm version*, so a future
algorithm change cannot silently collide with old IDs. Partial hashing is
near-instant even on large (300 MB+) archives and survives the archive being
moved or renamed. `book.path` is stored for display and relaunch only — if the
archive moves, the path goes stale but progress is not lost.

#### Schema

```json
{
  "schema": 1,
  "book": {
    "id": "p1:9f2a4c...e4",
    "path": "/games/games/starsector/manga/One Piece v01.cbz",
    "page_count": 210,
    "title": "One Piece v01"
  },
  "progress": {
    "page": 47,
    "completed": false,
    "open_count": 5,
    "first_opened": "2026-05-16T10:00:00Z",
    "last_read": "2026-05-16T11:23:00Z"
  },
  "view": {
    "mode": "paged",
    "direction": "rtl",
    "fit": "smart",
    "double_page": true,
    "cover_alone": true,
    "pair_offset": 0,
    "auto_trim": false,
    "snap": false
  },
  "spread_overrides": { "12": "pair", "13": "single" }
}
```

#### Field reference

| Field | Type | Notes |
|---|---|---|
| `schema` | int | Schema version; reader migrates older files on load. |
| `book.id` | string | Partial-hash identity key (see above). |
| `book.path` | string | Last known absolute path; display/relaunch only. |
| `book.page_count` | int | Total pages in the archive. |
| `book.title` | string | Display title (derived from filename for now). |
| `progress.page` | int | **Absolute** page index — not a spread index. The view layer re-pairs for double-page mode on its own. |
| `progress.completed` | bool | Set true on reaching the last page. |
| `progress.open_count` | int | Plain incrementer; +1 per reader launch of this book. |
| `progress.first_opened` | string | ISO-8601 UTC; set once. |
| `progress.last_read` | string | ISO-8601 UTC; updated on each save. |
| `view.mode` | enum | `paged` \| `scroll`. |
| `view.direction` | enum | `rtl` \| `ltr`. |
| `view.fit` | enum | `width` \| `height` \| `whole` \| `original` \| `smart`. |
| `view.double_page` | bool | Double-page spread on/off. |
| `view.cover_alone` | bool | First page unpaired in double-page mode. |
| `view.pair_offset` | int | Nudge applied to spread pairing. |
| `view.auto_trim` | bool | Crop uniform border whitespace. |
| `view.snap` | bool | Scroll-mode snap-to-page-boundary. |
| `spread_overrides` | object | Sparse map: page index (string key) → `single` \| `pair`. |

Consumer split: the library reads `book` + `progress` to render a shelf; it
never needs `view` or `spread_overrides`, which are reader-internal. Zoom/pan
is per-session and is deliberately **not** persisted.

---

## Source layout

Header skeletons exist under `src/`; `.cpp` implementations are pending. Each
top-level directory maps to one module from the Architecture section. Both
view widgets derive from a `QGraphicsView`-based `ReaderView` base — one
toolkit, shared zoom/pan and 2-zone click logic (no QML).

```
src/
  core/      types.h natural_sort.h          shared enums + utilities, no deps
  archive/   comic_archive.h                 libarchive wrapper, no GUI
  model/     page.h book.h                   page list, detection, no GUI
  decode/    decoded_image.h page_cache.h    async decode + LRU cache
             decode_service.h
  view/      reader_view.h                   QGraphicsView base
             paged_view.h scroll_view.h      the two reading widgets
  session/   book_identity.h progress_file.h partial-hash id, progress JSON,
             app_config.h                    global config
  app/       cli_options.h mode_resolver.h   arg parsing, mode precedence,
             chrome_controller.h             chrome visibility policy,
             main_window.h main.cpp          top-level window + entry point
CMakeLists.txt                               Qt6 Widgets + libarchive
```

Dependency direction is one-way: `core` ← `archive` ← `model` ← `decode` ←
`view`, with `session` and `app` on top. `archive` and `model` never link Qt
GUI, so they stay unit-testable in isolation.

## v1 MVP definition

Open CBZ/CBR → natural-sort pages → paged (single + double) and vertical
scroll modes → RTL/LTR → fit modes → prefetch cache → essential + high-value
view options → write progress file → accept launch args.

No library, no metadata browser. A complete, genuinely useful tool on its own.

---

## ComicInfo.xml

The archive layer **detects** a `ComicInfo.xml` member and stashes its raw
bytes (`ComicArchive::hasComicInfo` / `comicInfoXml`), but v1 does **not**
parse or act on it — reading direction and spread treatment come purely from
aspect-ratio heuristics and user/CLI input. Stashing it now means a future
library or metadata layer can consume it (the `Manga` RTL hint, per-page
`Type`/`DoublePage` attributes) with no change to the reader.

## Resolved decisions

- **Archive formats**: v1 = CBZ + CBR only; CB7/CBT deferred (zero-cost to add
  later via `libarchive`).
- **RAR coverage**: `libarchive` reads RAR4 and RAR5; no second archive
  library needed.
- **ComicInfo.xml**: detect and stash only; no parsing in v1 (see above).

## Status

**v1 is implemented.** All seven modules are built and verified; each has a
smoke test under `tests/` (`build/<module>_smoketest`), with the GUI tests
running headless via `QT_QPA_PLATFORM=offscreen`. `build/ur-reader
<archive.cbz>` opens a working reader window.

Build: `cmake -S . -B build && cmake --build build` (needs Qt6 + libarchive).

Known v1 simplifications, left for later:

- `auto_trim` and `ComicInfo.xml` parsing are persisted/stashed but not yet
  acted on.
- The scrubber has no thumbnail-preview-on-hover.
- Scroll-mode `snap` is stored but not yet enforced (free-scroll only).
- `ComicArchive::readEntry` re-opens the archive per call; a sequential-read
  cursor would speed up RAR.
- Startup mode auto-detection probes only the first 10 page headers.
