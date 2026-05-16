#pragma once

#include <QColor>
#include "core/types.h"

namespace ur {

// Global, cross-book settings — the global config file. Distinct from the
// per-book ProgressData. See DESIGN.md "Persistence split".
//
// MODULE: session/progress.
struct AppConfig {
    QColor      backgroundColor = QColor(40, 40, 40);   // default dark gray
    int         pageGap = 8;                            // px, spreads & scroll
    ReadingMode defaultMode = ReadingMode::Paged;       // last-resort fallback

    qint64      cacheBudgetBytes = 512LL * 1024 * 1024; // decoded-image budget
    int         prefetchAhead  = 4;
    int         prefetchBehind = 2;

    static AppConfig load();        // from AppConfigLocation; defaults if absent
    bool         save() const;
};

} // namespace ur
