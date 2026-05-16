#pragma once

#include <optional>
#include "core/types.h"

namespace ur {

class Book;

// Applies the reading-mode precedence from DESIGN.md:
//
//   1. CLI --mode argument        (highest priority)
//   2. saved progress file        (per-book remembered mode)
//   3. auto-detection             (Book::detectMode)
//   4. config default             (fallback)
//
// MODULE: app shell.
struct ModeResolver {
    static ReadingMode resolve(std::optional<ReadingMode> cli,
                               std::optional<ReadingMode> saved,
                               const Book&                book,
                               ReadingMode                configDefault);
};

} // namespace ur
