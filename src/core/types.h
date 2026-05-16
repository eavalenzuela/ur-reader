#pragma once

// Shared value types with no dependencies. Everything else may include this;
// this includes nothing of ours. Keeps the model free of any view coupling.

namespace ur {

enum class ReadingMode {
    Paged,    // single / double-page spreads
    Scroll,   // continuous vertical scroll (webtoon)
};

enum class ReadingDirection {
    LeftToRight,   // western
    RightToLeft,   // manga
};

enum class FitMode {
    Width,      // fit page width to viewport
    Height,     // fit page height to viewport
    Whole,      // fit the whole page
    Original,   // 1:1 pixels
    Smart,      // Whole, but Width for unusually tall pages
};

// Per-page user override of automatic double-spread detection.
enum class SpreadOverride {
    Auto,          // use width>height auto-detection
    ForceSingle,   // never pair this page
    ForcePair,     // always treat this page as a standalone spread
};

} // namespace ur
