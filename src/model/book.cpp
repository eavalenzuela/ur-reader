#include "model/book.h"

#include <QFileInfo>
#include <algorithm>

#include "archive/comic_archive.h"

namespace ur {

Book::Book(ComicArchive* archive)
    : m_archive(archive)
{
    if (!m_archive)
        return;

    m_title = QFileInfo(m_archive->sourcePath()).completeBaseName();

    const QVector<ArchiveEntry>& entries = m_archive->entries();
    m_pages.reserve(entries.size());
    for (const ArchiveEntry& entry : entries) {
        Page p;
        p.index = static_cast<int>(m_pages.size());
        p.entryIndex = entry.index;
        p.name = entry.name;
        m_pages.append(p);
    }
}

const QString& Book::title() const
{
    return m_title;
}

int Book::pageCount() const
{
    return static_cast<int>(m_pages.size());
}

Page& Book::page(int index)
{
    return m_pages[index];
}

const Page& Book::page(int index) const
{
    return m_pages[index];
}

void Book::setPageSize(int index, QSize size)
{
    if (index < 0 || index >= m_pages.size())
        return;
    if (!size.isValid() || size.isEmpty())
        return;
    m_pages[index].pixelSize = size;
    m_pages[index].sizeKnown = true;
}

std::optional<ReadingMode> Book::detectMode() const
{
    constexpr int    kMinSamples = 5;
    constexpr double kTallRatio  = 2.5;   // height / width threshold for a strip

    int samples = 0;
    int tall = 0;
    for (const Page& p : m_pages) {
        if (!p.sizeKnown || p.pixelSize.width() <= 0 || p.pixelSize.height() <= 0)
            continue;
        ++samples;
        const double ratio =
            static_cast<double>(p.pixelSize.height()) / p.pixelSize.width();
        if (ratio >= kTallRatio)
            ++tall;
    }

    // Need a reasonable sample — every page if the book is short, else kMin.
    const int needed = std::min(kMinSamples, pageCount());
    if (samples == 0 || samples < needed)
        return std::nullopt;

    // Webtoon when a clear majority of sampled pages are very tall strips.
    return (tall * 2 > samples) ? ReadingMode::Scroll : ReadingMode::Paged;
}

ComicArchive* Book::archive() const
{
    return m_archive;
}

} // namespace ur
