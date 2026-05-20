#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>
#include <optional>

#include "core/types.h"

namespace ur {

// Value of the ComicInfo.xml <Manga> element. The four values come straight
// from the spec; "Yes" on its own means "this is a manga" but is ambiguous
// about direction (Korean / Chinese manhwa can be LTR), so we treat only
// "YesAndRightToLeft" as a confident RTL hint.
enum class MangaHint {
    Unknown,
    No,
    Yes,
    YesAndRTL,
};

// Per-page metadata from <Pages><Page Image="N" .../></Pages>.
struct ComicInfoPage {
    bool    doublePage = false;
    QString type;             // FrontCover / Story / Advertisement / ...
};

// Best-effort ComicInfo.xml summary. Anything we can't parse is silently
// dropped — bad metadata never blocks reading.
//
// MODULE: book model. Uses QXmlStreamReader (Qt Core only).
struct ComicInfoHints {
    MangaHint                 manga = MangaHint::Unknown;
    QHash<int, ComicInfoPage> pages;     // sparse, keyed by <Page Image="N">

    static ComicInfoHints parse(const QByteArray& xml);

    // Reading direction implied by the <Manga> value, if confident.
    std::optional<ReadingDirection> readingDirection() const;

    bool empty() const
    {
        return manga == MangaHint::Unknown && pages.isEmpty();
    }
};

} // namespace ur
