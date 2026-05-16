#pragma once

#include <QString>
#include <QSize>
#include "core/types.h"

namespace ur {

// One readable page in the book.
struct Page {
    int     index = -1;        // 0-based absolute page index
    int     entryIndex = -1;   // index into ComicArchive::entries()
    QString name;              // entry name (display / sort)

    QSize   pixelSize;         // intrinsic size; filled lazily
    bool    sizeKnown = false; // true once probed or decoded

    SpreadOverride override = SpreadOverride::Auto;

    // True when intrinsic art is a wide double-spread (width > height).
    // Meaningful only when sizeKnown.
    bool isWideAuto() const;

    // Effective spread treatment: combines isWideAuto() with `override`.
    bool treatAsSpread() const;
};

} // namespace ur
