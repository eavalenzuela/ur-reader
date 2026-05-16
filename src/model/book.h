#pragma once

#include <QVector>
#include <QString>
#include <optional>
#include "model/page.h"
#include "core/types.h"

namespace ur {

class ComicArchive;

// The ordered set of pages plus book-level state. No Qt GUI.
// Does not own the ComicArchive — the app shell does.
//
// MODULE: book model (independently testable, no GUI).
class Book {
public:
    explicit Book(ComicArchive* archive);

    const QString& title() const;        // derived from filename for now
    int            pageCount() const;

    Page&          page(int index);
    const Page&    page(int index) const;

    // Records a probed/decoded pixel size, enabling spread & mode detection.
    // Called as pages are decoded lazily.
    void setPageSize(int index, QSize size);

    // Heuristic reading-mode guess from sampled page aspect ratios.
    // Returns std::nullopt until enough pages have known sizes.
    std::optional<ReadingMode> detectMode() const;

    ComicArchive* archive() const;

private:
    ComicArchive* m_archive;
    QString       m_title;
    QVector<Page> m_pages;
};

} // namespace ur
